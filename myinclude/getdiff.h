#ifndef GETDIFF_H_
#define GETDIFF_H_

#include "configure.h"
#include <curl/curl.h>
#include <time.h>

/* version number is a string! **/
/* #define VERSION "0.01.0"
 * removed global variable from curlfn.c
 * static CURLU   *parseUrlHandle = NULL;
 * added parameter "parseHandle" to:
 * download2File()
 * download2FileRetry()
 * getCurrentURL()
 * 11/20/2023
 *
 * changes in 0.01.2
 *  - function name change: getCurrentURL() -> parsePrefixCURLU()
 *  - getCurlResponseCode() -> responseCode2ztCode(long resCode) edited function
 *  - handles curl errors for retry(): CURLE_COULDNT_CONNECT
 *  - added more ztResponseNNN and other zt codes.
 *  date 12/6/2023
 *
 * changes in 0.01.3
 *  - replaced initialDownload() & initialQuery() with initialOperation()
 *  - improved error messages and logging
 *  - added documentation and worked on code comments.
 *  date 12/9/2023
 *
 * changes in 0.01.4
 *  - fixed "removeSpaces()" function
 *  - added zapString() - used in initialStringList() now.
 *  - complete implementation of STRING_LIST included.
 *  - removed other list types - not used here.
 *    (included in "primitives.h" for other projects)
 *  - added more error codes
 *  date 1/14/2024
 *
 *  updated 1/29/2024
 *   - updated lastOfPath() ---> util.c
 *   - added arg2FullPath() ---> util.c
 *   - use double digit for patch number in version number.
 *
 *  update 2/4/2024
 *   - modified removeSpaces() - do not copy overlapped strings
 *   - fixed getNewDiffersList() error.
 *   - added ztStringNotFound error code.
 *
 *  update 2/16/2024
 *    - Use updated python script from Geofabrik "oauth_cookie_client.py"
 *    - Use option CURLOPT_FOLLOWLOCATION - allow redirect
 *
 *********************************/

//#define VERSION "0.01.43" up to "0.01.77"
#define VERSION "0.01.77"

/* maximum allowed number of change files to download per invocation **/
#define MAX_OSC_DOWNLOAD 61
#define SLEEP_INTERVAL 1

#ifndef MAX_USER_NAME
#define MAX_USER_NAME 64
#endif

#define PATH_PART_LENGTH 1024

extern  char    *progName;
extern  int     fVerbose;
extern  FILE   *fLogPtr;


typedef struct SKELETON_ {

  char *workDir;
  char *tmp;

  char *geofabrik;
  char *planet;
  char *planetMin;
  char *planetHour;
  char *planetDay;

} SKELETON;

typedef struct GD_FILES_ { // get differ files

  // non-temporary files
  char *lockFile;
  char *logFile;
  char *previousSeqFile; // ID; sequence number
  char *prevStateFile;   // member to be removed
  char *newDiffersFile;
  char *rangeFile;

  // temporary files
  char *latestStateFile;

} GD_FILES;

typedef struct MY_SETTING_ {

  char *source;
  char *rootWD;
  char *configureFile;
  char *usr;
  char *pswd;
  char *startNumber;
  char *endNumber;

  int verbose;
  int newDifferOff;

  int textOnly;

} MY_SETTING;

typedef struct URL_PARTS_ { // not used?
	char scheme[8];
	char server[64];
	char dpage[64]; // differs page (where latest state.txt file is found)
} URL_PARTS;

#define STATE_EXT ".state.txt"
#define CHANGE_EXT ".osc.gz"

typedef struct PATH_PART_ {

  char sequenceNum[10];

  char rootEntry[5];  /* 3 characters (digits) + ending slash character
                         for both rootEntry & parentEntry. **/
  char parentEntry[5];
  char fileEntry[4];  /* fileEntry does NOT end with slash **/

  char parentPath[10]; /* Parent Page / differs directory listing **/
  char filePath[16];

} PATH_PART;

typedef struct STATE_INFO_ {

  char timeString[24];
  char seqNumStr[10];
  char originalSeqStr[10];

  int  isGeofabrik;

  PATH_PART *pathPart;
  struct tm *timestampTM;
  time_t    timeValue;

} STATE_INFO;

int getSettings(MY_SETTING *settings, int argc, char* const argv[]);

int mergeConfigure(MY_SETTING *settings, CONF_ENTRY confEntries[]);

int setupFilesys(SKELETON *directories, GD_FILES *files, const char *root);

int buildDirectories(SKELETON *dir, const char *where);

int setFilenames(GD_FILES *gdFiles, SKELETON *dir);

int chkRequired(MY_SETTING *settings, char *prevStateFile);

int isSourceSupported(char const *source);

int isSupportedScheme(const char *scheme);

int isSupportedServer(const char *servername);

int isPlanetPath(const char *path);

int isGeofabrikPath(const char *path);

char *setDiffersDirPrefix(SKELETON *skl, const char *src);

char *getLoginToken(MY_SETTING *setting, SKELETON *myDir);

PATH_PART *initialPathPart(void);

void zapPathPart(void **pathPart);

STATE_INFO *initialStateInfo();

void zapStateInfo(STATE_INFO **si);

int stateFile2StateInfo(STATE_INFO *stateInfo, const char *filename);

int isStateFileList(STRING_LIST *list);

char *stateFile2SequenceString(const char *filename);

int sequence2PathPart(PATH_PART *pathPart, const char *sequenceStr);

int isGoodSequenceString(const char *string);

int myDownload(char *remotePathSuffix, char *localFile);

char *fetchLatestSequence(char *remoteName, char *localDest);

int areNumsGoodPair(const char *startNum, const char *endNum);

int isRemoteFile(char *remoteSuffix);

int areAdjacentStrings(char *sourceSuffix, char *firstStr, char *secondStr);

int isEndNewer(PATH_PART *startPP, PATH_PART *endPP);

int makeOsmDir(PATH_PART *startPP, PATH_PART *latestPP, const char *rootDir);

int getDiffersList(STRING_LIST *destList, PATH_PART *startPP, PATH_PART *endPP);

int downloadFilesList(STRING_LIST *completed, STRING_LIST *downloadList, char *localDestPrefix, int textOnly);

int getParentPage(STRING_LIST *destList, char *parentSuffix);

int prependGranularity(STRING_LIST **list, char *what);


#endif /* GETDIFF_H_ */
