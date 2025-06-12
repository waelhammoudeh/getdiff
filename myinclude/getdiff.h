#ifndef GETDIFF_H_
#define GETDIFF_H_


#include <curl/curl.h>
//#include <time.h>
#include "gd_primitives.h"
#include "configure.h"


/* version number is a string! **/
/* #define VERSION "0.01.0"
 *
 *********************************/

//#define VERSION "0.01.43" up to "0.01.77"
//#define VERSION "0.01.77"
#define VERSION "0.01.81"

/* constant strings; I use defines for them **/
#define WORK_ENTRY         "getdiff"
#define CONF_NAME          "getdiff.conf"

#define LOCK_FILE          ".lock.getdiff"
#define LOG_NAME           "getdiff.log"
#define PREV_SEQ_FILE      "previous.seq"

/* previous.state.txt to be removed **/
#define PREV_STATE_FILE    "previous.state.txt"

#define NEW_DIFFERS        "newerFiles.txt"
#define RANGE_FILE         "rangeList.txt"
#define LATEST_STATE_FILE  "latest.state.txt"

#define HTML_EXT            ".html"

/* "state.txt" is remote filename **/
#define STATE_FILE         "state.txt"

/* extensions for state text and change files **/
#define STATE_EXT ".state.txt"
#define CHANGE_EXT ".osc.gz"

/* TEST_SITE test connection; list? google, osm & geofabrik **/
#define TEST_SITE          "www.geofabrik.de"
#define INTERNAL_SERVER    "osm-internal.download.geofabrik.de"


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


int getSettings(MY_SETTING *settings, int argc, char* const argv[]);

int mergeConfigure(MY_SETTING *settings, CONF_ENTRY confEntries[]);

int chkRequired(MY_SETTING *settings, char *prevStateFile);

char *setDiffersDirPrefix(SKELETON *skl, const char *src);

char *getLoginToken(MY_SETTING *setting, SKELETON *myDir);

int isStateFileList(STRING_LIST *list);

int isGoodSequenceString(const char *string);

int myDownload(char *remotePathSuffix, char *localFile);

char *fetchLatestSequence(char *remoteName, char *localDest);

int areNumsGoodPair(const char *startNum, const char *endNum);

int isRemoteFile(char *remoteSuffix);

int areAdjacentStrings(char *sourceSuffix, char *firstStr, char *secondStr);

int isEndNewer(PATH_PART *startPP, PATH_PART *endPP);

int getDiffersList(STRING_LIST *destList, PATH_PART *startPP, PATH_PART *endPP);

int downloadFilesList(STRING_LIST *completed, STRING_LIST *downloadList, char *localDestPrefix, int textOnly);

int getParentPage(STRING_LIST *destList, char *parentSuffix);

int prependGranularity(STRING_LIST **list, char *gString);

int isSourceSupported(char const *source, const CURLU *cParseHandle);

int isSourceSupported_old(char const *source);

int isSupportedServer(const char *server);

int isPlanetPath(const char *path);

int isGeofabrikPath(const char *path);


#endif /* GETDIFF_H_ */
