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

#define VERSION "0.01.43"

/* maximum allowed number of change files to download per invocation **/
#define MAX_OSC_DOWNLOAD 61
#define SLEEP_INTERVAL 1

#ifndef MAX_USER_NAME
#define MAX_USER_NAME 64
#endif

#define PATH_PART_LENGTH 1024

extern  char    *progName;
extern  int     fVerbose;

typedef struct SETTINGS_ {

  char   *usr;
  char   *pswd;
  char   *source;
  char   *rootWD;
  char   *workDir;
  char   *diffDir;
  char   *confFile;
  char   *logFile;
  char   *newDiffersFile;

  char   *scriptFile;
  char   *jsonFile;
  char   *cookieFile;

  char   *startNumber;

  char   *prevStateFile;
  char   *latestStateFile;
  char   *prevSeqFile;

  char   *htmlFile;

  char   *tstSrvr;
  int    noNewDiffers;
  int    verbose;
  int    textOnly; /* flag, when set download 'state.txt' files only; no change files! **/

} SETTINGS;


typedef struct PATH_PARTS_ {

  char   path[13];
  char   childPath[10];
  char   parentPath[6];

  char   parentEntry[4];
  char   childEntry[4];
  char   file[4];

} PATH_PARTS;


typedef struct STATE_INFO_{

  struct tm timestampTM;
  char      *sequenceNumber;
  char      *originalSequenceOSM; /* not used **/

  time_t     timeValue;
  PATH_PARTS *pathParts;

} STATE_INFO;


void printSettings(FILE *toFile, SETTINGS *settings);

char *appendName2Path(char const *path, char const *name);

int updateSettings (SETTINGS *settings, CONF_ENTRY confEntries[]);

int setFilenames(SETTINGS *settings);

int isOkaySource(CURLU *curlSourceHandle);

int logMessage(FILE *to, char *msg);

int stateFile2StateInfo(STATE_INFO *stateInfo, const char *filename);

int isStateFileList(STRING_LIST *list);

int parseTimestampLine(struct tm *tmStruct, char *timeString);

int parseSequenceLine(char **sequenceString, const char *line);

int isGoodSequenceString(const char *string);

int readStartID(char **idString, char *filename);

int writeStartID (char *idStr, char *filename);

//int myDownload(CURLU *parseHandle, CURL *downloadHandle, char *remotePathSuffix, char *localFile);
int myDownload(char *remotePathSuffix, char *localFile);

int seq2PathParts(PATH_PARTS *pathParts, const char *sequenceStr);

void fprintPathParts(FILE *file, PATH_PARTS *pp);

STATE_INFO *initialStateInfo();

int getNewDiffersList(STRING_LIST *downloadList, PATH_PARTS *startPP,
		             PATH_PARTS *latestPP, SETTINGS *settings, int includeStartFiles);

int insertFileWithPath(STRING_LIST *toList, ELEM *startElem, char *path);

int downloadFiles(STRING_LIST *completed, STRING_LIST *downloadList, SETTINGS *settings);

int makeOsmDir(PATH_PARTS *startPP, PATH_PARTS *latestPP, const char *rootDir);

void printfWriteMsg(char *msg,FILE *tof);

int writeNewerFiles(char const *toFile, STRING_LIST *list);

int getHeader(const char *tofile, const char *pathSuffix);

int getListHeaders(STRING_LIST *list, SETTINGS *settings);



#endif /* GETDIFF_H_ */
