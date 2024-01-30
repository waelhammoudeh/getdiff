/*
 * getdiff.c
 *
 *  Created on: Apr 14, 2023
 *      Author: Wael Hammoudeh
 *      email: w_hammoudeh@hotmail.com
 *
 *  Program: program to download OSC (OSM Change) files and their
 *  corresponding 'state.txt' files from the internet.
 *
 *******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "usage.h"
#include "ztError.h"
#include "getdiff.h"
#include "util.h"
#include "parse.h"
#include "configure.h"
#include "curlfn.h"
#include "cookie.h"
#include "fileio.h"
#include "tmpFiles.h"
#include "parseAnchor.h"

/* those names should not be changed; I use defines for them **/
#define WORK_ENTRY         "getdiff/"
#define DIFF_ENTRY         "diff/"
#define CONF_NAME          "getdiff.conf"
#define LOG_NAME           "getdiff.log"
#define NEW_DIFFERS        "newerFiles.txt"

#define SCRIPT_FILE        "oauth_cookie_client.py"
#define JSON_FILE          "settings.json"
#define COOKIE_FILE        "geofabrikCookie.txt"

#define PREV_SEQ_FILE      "previous.seq"
#define PREV_STATE_FILE    "previous.state.txt"
#define LATEST_STATE_FILE  "latest.state.txt"

#define STATE_FILE         "state.txt"
#define INDEX_HTML         "index.html"

#define TEST_SITE          "www.geofabrik.de"
#define INTERNAL_SERVER    "osm-internal.download.geofabrik.de"


/*global variables **/
char   *progName = NULL;
int    fVerbose = 0;
FILE   *fLogPtr = NULL;


/* curl easy handle and curl parse handle **/
static CURL   *downloadHandle = NULL;
static CURLU  *curlParseHandle = NULL;
static char   *sourceURL = NULL;

int main(int argc, char *argv[]){

  //printAcceptedSources(stdout);

  int  result;

  STRING_LIST   *childrenList;
  STRING_LIST   *newDiffersList;
  STRING_LIST   *completedList;

  /* set progName - used in output messages. **/
  progName = lastOfPath(argv[0]);

  /* set some sensible defaults we may have to use.
   * {HOME} is current user home directory.
   * default work directory: {HOME}/getdiff/
   * default configuration file: {HOME}/getdiff.conf
   *
   * Global 'fVerbose' is zero, we only turn it on when set by user.
   *
   ********************************************/
  char   *homeDir;
  char   *defWorkDir;
  char   *defConfFile;


  homeDir = getHome();

  if( ! homeDir ){
    fprintf(stderr, "%s: Error failed to set user home directory.\n", progName);
    return ztUnknownError;
  }

  defWorkDir = appendName2Path(homeDir, WORK_ENTRY);

  defConfFile = appendName2Path(homeDir, CONF_NAME);

  /* parse command line arguments first giving user chance to set the
   * configuration file, storing result in SETTINGS structure **/

  SETTINGS *mySettings;

  mySettings = (SETTINGS *)malloc(sizeof(SETTINGS));
  if( ! mySettings ){
    fprintf(stderr, "%s: Error allocating memory.\n", progName);
    return ztMemoryAllocate;
  }
  memset(mySettings, 0, sizeof(SETTINGS));

  /* mySettings are ALL zeroed out going in **/
  result = parseCmdLine(mySettings, argc, argv);
  if (result != ztSuccess){
    fprintf(stderr, "%s: Error failed to parse command line, see above error message. Exiting.\n", progName);
    return result;
  }

  if(!mySettings->confFile){ /* user did not set configuration file **/

    result = isFileUsable(defConfFile);

    if(result != ztSuccess)

      /* use default configuration file if it exists,
       * file is not required, but write warning if not found regardless of verbose settings **/
      fprintf(stdout, "%s: Warning configuration file is not specified and could not find or access"
	      " default configuration file:\n"
	      " <%s>\n Default file is not usable for: <%s>\n",
	      progName, defConfFile, ztCode2Msg(result));

    else

      mySettings->confFile = STRDUP(defConfFile); /* set ONLY when found - result == ztSuccess **/

  }

  int   haveConfArgs = (mySettings->usr && mySettings->pswd &&
                        mySettings->source && mySettings->workDir &&
                        mySettings->startNumber && mySettings->noNewDiffers &&
                        mySettings->verbose);

  /* configure array has ALL 'value' members in the array set to NULL - zeroed out **/
  CONF_ENTRY  confEntries[] = {
    {"USER", NULL, NAME_CT, 0},
    {"PASSWD", NULL, NAME_CT, 0},
    {"SOURCE", NULL, INET_URL_CT, 0},
    {"DIRECTORY", NULL, DIR_CT, 0},
    {"TEST_SITE", NULL, INET_URL_CT, 0},
    {"BEGIN", NULL, DIGITS9_CT, 0},
    {"VERBOSE", NULL, BOOL_CT, 0},
    {"NEWER_FILE", NULL, NONE_CT, 0}, /* NONE_CT accepts 'none' and 'off' for value **/
    {NULL, NULL, 0, 0}
  };

  /* skip processing configuration file when ALL arguments are given in the command line **/
  if(mySettings->confFile && ! haveConfArgs ){

    result = initialConf(confEntries, 8);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed initialConf() function for <%s>. Exiting.\n",
	      progName, ztCode2Msg(result));
      return result;
    }

    int numFound;

    confErrLineNum = 0; /* on error set by configureGetValue() **/

    result = configureGetValues(confEntries, &numFound, mySettings->confFile);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed configureGetValues() function for <%s>. Exiting.\n",
	      progName, ztCode2Msg(result));

      if(confErrLineNum)

        fprintf(stderr, "Error in configuration file at line number: <%d>\n",  confErrLineNum);

      return result;
    }

    /* updateSettings(): brings in values from configuration file to mySettings **/
    result = updateSettings(mySettings, confEntries);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed updateSettings() from configure file.\n", progName);
      return result;
    }

  } /* end if(... process configuration file ...) **/

  if (mySettings->verbose == 1){
    fVerbose = 1;
  }

  /* if source is not set, exit with error message.
   * source argument is verified farther down. **/
  if(! mySettings->source){
    fprintf (stderr, "%s: Error missing required 'source' URL argument.\n", progName);
    return ztMissingArg;
  }

  /* set program work directory **/
  if(mySettings->rootWD){

    result = isDirUsable(mySettings->rootWD);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed isDirUsable() function with root for work directory parameter: <%s>\n"
	      "User ownership of this directory is required.\n"
	      "function failed for: <%s>\n",
	      progName, mySettings->rootWD, ztCode2Msg(result));
      return result;
    }

    /* root for work directory can not end with "getdiff" entry **/
    char   *lastEntry;

    lastEntry = lastOfPath(mySettings->rootWD);
    if(strcmp(lastEntry,WORK_ENTRY) == 0){

      fprintf(stderr, "%s: Error root for work directory can not end with <%s> entry.\n",
	      progName, WORK_ENTRY);
      return ztInvalidArg;
    }

    mySettings->workDir = appendName2Path(mySettings->rootWD, WORK_ENTRY);
  }

  else{ /* user did not specify root for work directory, use default **/

    mySettings->workDir = STRDUP(defWorkDir);
  }

  /* set file names using workDir setting.
   * setFilenames() function terminates program on error **/

  setFilenames(mySettings);

  /* ensure work directory and differs directory exist **/
  result = myMkDir(mySettings->workDir);
  if(result != ztSuccess){

    fprintf(stderr, "%s: Error failed to create program work directory: <%s>.\n",
	    progName, mySettings->workDir);
    return result;
  }

  result = myMkDir(mySettings->diffDir);
  if(result != ztSuccess){

    fprintf(stderr, "%s: Error failed to create differs destination directory: <%s>.\n",
	    progName, mySettings->diffDir);
    return result;
  }

  /* open log file with 'append' mode - fLogPtr is global **/

  char   logBuffer[PATH_MAX] = {0};

  if(! mySettings->logFile){
    fprintf(stderr, "%s: Error log file name is not set.\n", progName);
    return ztUnknownError;
  }

  errno = 0;
  fLogPtr = fopen (mySettings->logFile, "a");
  if (! fLogPtr ){
    fprintf (stderr, "%s: Error could not open log file! <%s>\n",
	     progName, mySettings->logFile);
    fprintf(stderr, " System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  /* write log start header **/
  logMessage(fLogPtr, "START");

  memset(logBuffer, 0, sizeof(logBuffer));

  if(mySettings->confFile)
    sprintf(logBuffer, "Using configuration file: <%s>", mySettings->confFile);
  else
    sprintf(logBuffer, "Not using any configuration file");

  printfWriteMsg(logBuffer, stdout);

  /* if newer files is turned off by user AND we find an old file in
   * working directory, we rename it by appending ".old~" extension
   *
   ****************************************************************/

  if(mySettings->noNewDiffers){
    result = isFileUsable(mySettings->newDiffersFile);
    if(result == ztSuccess){

      char   newName[1024] = {0};

      sprintf(newName, "%s.old~", mySettings->newDiffersFile);

      result = renameFile(mySettings->newDiffersFile, newName);
      if(result == ztSuccess){
        memset(logBuffer, 0, sizeof(logBuffer));
        sprintf(logBuffer, "Newer files is set to 'None', renamed existing file to: <%s>", newName);
        printfWriteMsg(logBuffer, stdout);
      }
      else{
	printfWriteMsg("Error failed renameFile() function for 'newerFiles.txt'. Exiting", stderr);
	return result;
      }
    }
  }

  /* curl library functions are used to:
   *  1) parse source URL argument for verifications.
   *  2) download change files and their state.txt files from remote server.
   *  3) test internet connection. (does not require curl session, uses curl functions).
   *
   * EXIT_CLEAN : is a label to enable us to cleanup on error before exit.
   *****************************************************************************/

  int    value2Return = ztSuccess; /* value to return at EXIT_CLEAN **/

  char   *host = NULL;
  char   *path = NULL;

  CURLUcode   curluResult; /* returned type by curl_url_get() & curl_url_set() **/

  /* set global sourceURL **/
  sourceURL = STRDUP(mySettings->source);

  result = initialCurlSession();
  if (result != ztSuccess){
    printfWriteMsg("Error failed initialCurlSession() function.", stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }
  else
    printfWriteMsg("Initialed curl session okay.", stdout);

  /* let our curl functions log errors to our log file **/
  curlLogtoFP = fLogPtr;

  /* get curl parse handle using source argument and verify pointer **/
  curlParseHandle = initialURL(mySettings->source);
  if (! curlParseHandle ){
    printfWriteMsg("Error failed initialURL() function.", stderr);

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }
  else
    printfWriteMsg("Got curl parse handle with initialURL() function okay.", stdout);

  /* extract host - server name **/
  curluResult = curl_url_get(curlParseHandle, CURLUPART_HOST, &host, 0);
  if (curluResult != CURLUE_OK ) {
    printfWriteMsg("Error failed curl_url_get() for server part.", stderr);

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }
  else {
    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Extracted server name from source; server name is: <%s>", host);
    printfWriteMsg(logBuffer, stdout);
  }

  /* path part is needed further down, get it now */
  curluResult = curl_url_get(curlParseHandle, CURLUPART_PATH, &path, 0);
  if (curluResult != CURLUE_OK ) {
    printfWriteMsg("Error failed curl_url_get() for path part.", stderr);

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }
  else {
    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Extracted path part from source; path part is: <%s>", path);
    printfWriteMsg(logBuffer, stdout);
  }

  /* is source string okay?
   * isOkaySource(): verifies source by PARTS from parse handle **/

  result = isOkaySource(curlParseHandle);
  if(result != ztSuccess){
    printfWriteMsg("Error failed isOkaySource() function.", stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }
  else
    printfWriteMsg("Verified source string result okay from isOkaySource() function.", stdout);

  /* begin argument is required for program first time use only.
   * on exit program writes "previous.state.txt" file in its working
   * directory, it is program first run when this file is NOT found.
   *
   ************************************************************/

  int   firstUse;

  result = isFileUsable(mySettings->prevStateFile);

  firstUse = (result != ztSuccess);

  if(firstUse)
    printfWriteMsg("Program first run, did not find previous state file in working directory.", stdout);
  else
    printfWriteMsg("Found previous state file in working directory; not first run for program.", stdout);

  if(firstUse && (! mySettings->startNumber)){
    printfWriteMsg("Error missing 'begin' argument; "
		   "argument is required for program first use.", stderr);

    value2Return = ztMissingArg;
    goto EXIT_CLEAN;
  }

  /* user and passwd arguments are required when geofabrik.de restricted
   * internal server is in use.
   *
   ******************************************************************/

  int   useInternal; /* geofabrik internal server flag **/
  char  *secToken = NULL;

  useInternal = (strcasecmp(host, INTERNAL_SERVER) == 0);

  if(useInternal){

    printfWriteMsg("Geofabrik.de INTERNAL server is in use.", stdout);

    /* openstreetmap.org account user name & password are required **/

    if( ! mySettings->usr ){
      printfWriteMsg("Error missing 'user' argument setting, "
		     "'OSM USER' is required for geofabrik.de internal server.", stderr);

      value2Return = ztMissingArg;
      goto EXIT_CLEAN;
    }

    if( ! mySettings->pswd ){
      printfWriteMsg("Error missing 'passwd' argument setting, "
		     "password for 'OSM ACCOUNT' is required for geofabrik.de internal server.", stderr);

      value2Return = ztMissingArg;
      goto EXIT_CLEAN;
    }

    /* we get cookie after verifying internet connection **/

  } /* end if(.., INTERNAL_SERVER)*/

  /* besides settings from user, program requires internet connection; is it there? **/

  result = isConnCurl(mySettings->source);
  if(result != ztSuccess){
    printfWriteMsg("Error could not connect to server. This program requires internet connection.", stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }
  else
    printfWriteMsg("Tested internet connection to server with result okay.", stdout);

  /* done processing required arguments **/

  printfWriteMsg("Done collecting settings. Calling printSettings() to log file.\n", stdout);

  printSettings(fLogPtr, mySettings);

  if(useInternal){

    printfWriteMsg("Retrieving cookie for INTERNAL SERVER use.", stdout);

    result = initialCookie(mySettings);
    if(result != ztSuccess){
      printfWriteMsg("Error failed initialCookie() function.", stderr);

      memset(logBuffer, 0, sizeof(logBuffer));
      sprintf(logBuffer, "initialCookie() failed for: <%s>", ztCode2Msg(result));
      printfWriteMsg(logBuffer, stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }
    else
      printfWriteMsg("result from initialCookie() function was successful.", stdout);

    secToken = getCookieToken();
    if(!secToken){
      printfWriteMsg("Error failed getCookieToken() function.", stderr);

      value2Return = ztNoCookieToken;
      goto EXIT_CLEAN;
    }
    else
      printfWriteMsg("Result from getCookieToken() function was successful.", stdout);

    destroyCookie();

  } /* end if(useInternal) **/

  /* obtain CURL easy handle with initialOperation() with parameters
   * curlParseHandle and secToken.
   *
   * Note that those were made global variables
   * CURL   *downloadHandle = NULL;
   * CURLU  *curlParseHandle = NULL;
   *************************************************************************/

  downloadHandle = initialOperation(curlParseHandle, secToken);
  if( !downloadHandle ){
    printfWriteMsg("Error failed initialOperation() function.", stderr);

    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "initialOperation() function failed for: <%s>. Exiting... ", curlErrorMsg);
    printfWriteMsg(logBuffer, stderr);

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }
  else
    printfWriteMsg("obtained downloadHandle with initialDownload() function successfully.", stdout);

  /* set startSequence and latestSequence **/
  char   *startSequence;
  char   *latestSequence;

  STATE_INFO   *startStateInfo;
  STATE_INFO   *latestStateInfo;

  char   remotePathSuffix[PATH_PART_LENGTH] = {0};
  char   localDest[PATH_PART_LENGTH] = {0};

  /* we may need to get startSequence from 'previous.state.txt' file. **/
  startStateInfo = initialStateInfo();
  if(!startStateInfo){
    printfWriteMsg("Error failed initialStateInfo() function.", stderr);

    value2Return = ztMemoryAllocate;
    goto EXIT_CLEAN;
  }

  if(firstUse == TRUE){

    printfWriteMsg("Program first run; mapping start sequence number to path ...", stdout);

    /* convert 'startNumber' to PATH_PART so we can download the
     * corresponding "state.txt" file.
     *
     ************************************************************/

    PATH_PARTS   pathParts;

    result = seq2PathParts(&pathParts, mySettings->startNumber);
    if(result != ztSuccess){
      printfWriteMsg("Error failed seq2PathParts() function.", stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }

    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "start sequence number: <%s> maps to path: <%s>",
	    mySettings->startNumber, pathParts.path);
    printfWriteMsg(logBuffer, stdout);

    /* remotePathSuffex is FULL 'path' member of pathParts + {.state.txt}
     * localDest: {workDir} + 'file' member of pathParts + {.state.txt}
     * note the period in both statements below. **/
    sprintf(remotePathSuffix, "%s.%s", pathParts.path, STATE_FILE);
    sprintf(localDest, "%s%s.%s", mySettings->workDir, pathParts.file, STATE_FILE);

    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Fetching corresponding 'state.txt' file to start sequence using remote path suffix: <%s>",
	    remotePathSuffix);
    printfWriteMsg(logBuffer, stdout);

    result = myDownload(remotePathSuffix, localDest);
    if(result != ztSuccess){
      sprintf(logBuffer, "Error failed myDownload() function for start sequence '%s' file.",mySettings->startNumber);
      printfWriteMsg(logBuffer, stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }

    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Retrieved start sequence 'state.txt' file, saved to: <%s>", localDest);
    printfWriteMsg(logBuffer, stdout);

    /* fill start STATE_INFO from just downloaded file **/
    result = stateFile2StateInfo(startStateInfo, localDest); /* FIXME: file to be REMOVED **/
    if(result != ztSuccess){
      printfWriteMsg("Error failed stateFile2StateInfo() function for start sequence 'state.txt' file.", stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }
  }

  else{ /* not firstUse: get startSequence from 'previous.state.txt' file **/

    printfWriteMsg("Found previous 'state.txt', IGNORING 'begin' setting, filling start STATE_INFO ...", stdout);

    result = stateFile2StateInfo(startStateInfo, mySettings->prevStateFile);
    if(result != ztSuccess){
      memset(logBuffer, 0, sizeof(logBuffer));
      sprintf(logBuffer, "Error failed stateFile2StateInfo() function for 'previous.state.txt' file.\n"
	      "function failed for: %s\n"
	      "Note that this file might be corrupted or edited.", ztCode2Msg(result));
      printfWriteMsg(logBuffer, stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }
  }

  startSequence = startStateInfo->sequenceNumber;

  /* get latest state.txt file from server "region page".
   * latest state.txt file is always at argument to our "source" setting with file name "state.txt" **/
  result = myDownload(STATE_FILE, mySettings->latestStateFile);
  if(result == ztSuccess){
    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Retrieved latest 'state.txt' file successfully;\n"
	    "File saved to <%s>, file size: <%ld> bytes.",
	    mySettings->latestStateFile, sizeDownload);
    printfWriteMsg(logBuffer, stdout);
  }
  else{
    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Error failed myDownload() function for latest 'state.txt' file.\n"
	    " Function failed for: <%s>. Exiting...", ztCode2Msg(result));
    printfWriteMsg(logBuffer, stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }

  latestStateInfo = initialStateInfo();
  if(! latestStateInfo){
    printfWriteMsg("Error failed initialStateInfo() function.", stderr);

    value2Return = ztMemoryAllocate;
    goto EXIT_CLEAN;
  }

  /* parse state.txt file into STATE_INFO structure **/
  result = stateFile2StateInfo(latestStateInfo, mySettings->latestStateFile);
  if(result != ztSuccess){
    printfWriteMsg("Error failed stateFile2StateInfo() function for latest 'state.txt' file.", stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }

  latestSequence = latestStateInfo->sequenceNumber;

  memset(logBuffer, 0, sizeof(logBuffer));
  sprintf(logBuffer, "INFO:\n"
	  " Info: startSequence is set to: <%s> and maps to path: <%s>\n"
	  " Info: latestSequence is set to: <%s> and maps to path: <%s>",
	  startSequence, startStateInfo->pathParts->path,
	  latestSequence, latestStateInfo->pathParts->path);
  printfWriteMsg(logBuffer, stdout);

  /* bail out on an unexpected time values
   * time value for latest can NOT be less than time value for start **/
  if(latestStateInfo->timeValue < startStateInfo->timeValue){
    printfWriteMsg("Error LATEST timeValue is less than START timeValue.\n"
                   "LATEST is the newest sequence and START is the previous sequence.\n"
                   "Corrupted or possibly modified 'previous.state.txt' file!?", stderr);

    value2Return = ztUnknownError;
    goto EXIT_CLEAN;
  }
  else if( (latestStateInfo->timeValue == startStateInfo->timeValue) &&
	   (strcmp(latestStateInfo->sequenceNumber, startStateInfo->sequenceNumber) != 0) ){
    printfWriteMsg("Error LATEST timeValue is equal to START timeValue with different sequence numbers!\n"
		   "Corrupted or possibly modified 'previous.state.txt' file!?", stderr);

    value2Return = ztUnknownError;
    goto EXIT_CLEAN;
  }

  /* verify that start and latest sequence numbers are for same GRANULARITY:
   * minute --> has 7 digit sequence number (5652032)
   * hour --> has 5 digit sequence number (94824)
   * day --> has 4 digit sequence number (3951)
   * geofabrik.de has 'day' update (4 digit sequence number).
   *
   * Those numbers may change; but not suddenly.
   * start and latest sequence numbers have same number of digits usually;
   * or one digit difference --> 99 to 100.
   * Do NOT mix minutely with hourly or daily change files etc.
   *******************************************************************/
  if(abs(strlen(latestSequence) - (strlen(startSequence)) > 1)){
    printfWriteMsg("Error large difference between string length of latest and previous sequence numbers!\n"
		   "Possible mixing of time granularities? Exiting.", stderr);

    value2Return = ztInvalidArg;
    goto EXIT_CLEAN;
  }

  /* Do NOT allow huge change in sequence number.
   * When sequence number is formated in 9 digits with leading zeros; the three
   * most significant numbers should be the same: (5652032 --> 005652032)
   * both should start with '005'.
   * start sequence and latest sequence MUST share root directory on the server;
   * that is the "parentEntry" member in our PATH_PART structure should be the same.
   ******************************************************************************/
  if(strcmp(latestStateInfo->pathParts->parentEntry, startStateInfo->pathParts->parentEntry) != 0){
    printfWriteMsg("Error start and latest sequence numbers do NOT map to the same "
		   "parent (root) directory on the server.\n"
		   "This could be your start sequence is way too far back from latest sequence.\n"
		   "Please download more recent area data file in this case.\n"
		   "This is also could be mixing of time granularity. Exiting.", stderr);

    value2Return = ztInvalidArg;
    goto EXIT_CLEAN;
  }

  /* limit how far apart start and latest sequence numbers are:
   * if latest and start sequence numbers do not share (come from)
   * the same "childEntry" directory on the server; then they must
   * come from consecutive "childEntry" directories on the server;
   * that is latest sequence "childEntry" directory must be NEXT TO
   * start sequence "childEntry" directory in sorted directory list.
   *
   * A directory has 1000 change files and their state.txt files,
   * the time period will vary depending on time granularity.
   * 1000 minute = 16:40 (16 hours + 40 minutes)
   * 1000 hour --> about 41 day and 16 hours
   * 1000 day --> about 2 years and 9 months.
   *
   * You only have to think about this limit when your data is more
   * than THREE years old.
   * This limit does NOT apply to geofabrik.de server since they only
   * offer daily updates and for the previous THREE months period or
   * about some 90+ change files for each region - well under 1000.
   *
   * To verify directories are next to each other, we get the directory
   * listing for the "parent" HTML page, parse this page into entries,
   * then we place those entries into a sorted string list. We check
   * that latest entry directory is the one next to start entry directory.
   *
   **************************************************************************/

  if(strcmp(latestStateInfo->pathParts->childEntry,
	    startStateInfo->pathParts->childEntry) != 0){

    printfWriteMsg("Start sequence and latest sequence numbers did not come from the same "
		   "server directory. Getting server directory listing page ...", stdout);

    result = myDownload(latestStateInfo->pathParts->parentPath, mySettings->htmlFile);
    if(result != ztSuccess){
      printfWriteMsg("Error failed myDownload() function for 'parent' index page.", stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }

    printfWriteMsg("Retrieved 'parent directory listing', calling parseHtmlFile() ...", stdout);

    childrenList = initialStringList();

    if(!childrenList){
      printfWriteMsg("Error failed initialStringList().", stderr);

      value2Return = ztMemoryAllocate;
      goto EXIT_CLEAN;
    }

    result = parseHtmlFile(childrenList, mySettings->htmlFile);
    if(result != ztSuccess){
      printfWriteMsg("Error failed parseHtmlFile() function", stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }

    /* file parsed okay, delete it **/
    removeFile(mySettings->htmlFile);

    ELEM   *elemLatest, *elemStart;

    elemLatest = findElemSubString(childrenList, latestStateInfo->pathParts->childEntry);

    elemStart = findElemSubString(childrenList, startStateInfo->pathParts->childEntry);

    if( ! (elemLatest && elemStart) ){
      printfWriteMsg("Error failed to find latest or start directory entry in directory listing.", stderr);

      value2Return = ztUnknownError;;
      goto EXIT_CLEAN;
    }

    /* latest MUST be next to start **/
    if(elemLatest != DL_NEXT(elemStart)){

      printfWriteMsg("Error start sequence number is too far from latest sequence!\n"
		     "Need more recent source. If you are using minute or hour, try daily first to bring\n"
		     "your data closer to latest sequence number.", stderr);

      value2Return = ztInvalidArg;
      goto EXIT_CLEAN;
    }
  } /* end if( ... not same directory ..) **/

  /* make file system structure under our differ directory.
   * We follow 'osm.org' organization here using the sequence
   * number formated into nine digits with leading zeros.
   * Directories are made as needed.
   * Update script (program) may remove files AND directories.
   ************************************************************/

  result = makeOsmDir(startStateInfo->pathParts, latestStateInfo->pathParts, mySettings->diffDir);
  if(result != ztSuccess){
    printfWriteMsg("Error failed makeOsmDir() function.", stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }

  printfWriteMsg("Local destination directory is ready for new differs download.", stdout);

  /* now startSequence number and latestSequence number are okay, make the download list **/

  /* initial download list **/
  int includeStartFlag = 0; /* do not include start sequence files.
                               'start' is really our previous - most recent -
                               downloaded files; we start from files just
                               after this. **/

  newDiffersList = initialStringList();

  if(!newDiffersList){
    printfWriteMsg("Error failed initialStringList().", stderr);

    value2Return = ztMemoryAllocate;
    goto EXIT_CLEAN;
  }

  if(firstUse){

    includeStartFlag = 1; /* start newDiffersList WITH start files **/

    /* special case: firstUse AND (latest sequence == start sequence)
     * time values will be the same, call getNewDiffersList() here it
     * will not get called in the next block.
     *****************************************************************/
    if(strcmp(latestSequence, startSequence) == 0){
      printfWriteMsg("Program first run with LATEST sequence number equals"
		     " START sequence number.", stdout);

      result = getNewDiffersList(newDiffersList,
				 startStateInfo->pathParts, latestStateInfo->pathParts,
				 mySettings, includeStartFlag);

      if(result != ztSuccess){
        printfWriteMsg("Error failed getNewDiffersList() function.", stderr);

        value2Return = result;
        goto EXIT_CLEAN;
      }
    }
  }
  else { /* not firstUse **/

    if(strcmp(latestSequence, startSequence) == 0){
      printfWriteMsg("No new differs to get; latest sequence number "
		     "equals start sequence number. Exiting", stdout);

      /* write this message to terminal if verbose is off **/
      if(fVerbose == 0)
	fprintf(stdout, "%s: No new differs to get; latest sequence number "
		"equals start sequence number. Exiting\n", progName);

      value2Return = ztSuccess;
      goto EXIT_CLEAN;
    }
  }

  if(latestStateInfo->timeValue > startStateInfo->timeValue){

    result = getNewDiffersList(newDiffersList,
			       startStateInfo->pathParts, latestStateInfo->pathParts,
			       mySettings, includeStartFlag);

    if(result != ztSuccess){
      printfWriteMsg("Error failed getNewDiffersList() function.", stderr);

      memset(logBuffer, 0, sizeof(logBuffer));
      sprintf(logBuffer, " Function getNewDiffersList() failed for: < %s >", ztCode2Msg(result));
      printfWriteMsg(logBuffer, stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }
  }

  if(DL_SIZE(newDiffersList) == 0){ /* we think it should not happen **/
    printfWriteMsg("Error getNewDiffersList() returned an EMPTY list with latest timeValue > start timeValue!\n"
		   "This is UNKNOWN ERROR.", stderr);

    value2Return = ztUnknownError;
    goto EXIT_CLEAN;
  }
  else{
    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Filled newDiffersList with size: <%d>\n Printing List:", DL_SIZE(newDiffersList));
    printfWriteMsg(logBuffer, stdout);

    fprintStringList(fLogPtr, newDiffersList);
    if(fVerbose)
      fprintStringList(NULL, newDiffersList);
  }

  /*
    char   *newDiffersListFile;

    newDiffersListFile = appendName2Path(mySettings->workDir, "newDiffersList.NoTrim.txt");

    result = stringList2File(newDiffersListFile, newDiffersList);
    if(result == ztSuccess){
    printfWriteMsg("Wrote NON-TRIMMED new differs to 'newDiffersList.NoTrim.txt' file.", stdout);
    }
  **/

  /** newDiffersList: each line is in an element

      /005/637/617.osc.gz
      /005/637/617.state.txt
      /005/637/618.osc.gz
      /005/637/618.state.txt
      /005/637/619.osc.gz
      /005/637/619.state.txt

  **/

  /* do not download more than MAX_OSC_DOWNLOAD in one session ... trim list **/
  if(DL_SIZE(newDiffersList) > (MAX_OSC_DOWNLOAD * 2)){

    fprintf(stdout, "New differs list size is: <%d> change files; trimming to maximum allowed per run <%d>\n"
	    "Please wait at least 60 minutes for a rerun to fetch the rest.\n"
	    "This is so we do not overwhelm the server and to avoid sending too many requests.\n",
	    (DL_SIZE(newDiffersList) / 2), MAX_OSC_DOWNLOAD);

    printfWriteMsg("Trimming new differs list to 61 change files. "
		   "Please wait at least 60 minutes before a rerun.", stdout);

    char   *newDiffersListFile;

    newDiffersListFile = appendName2Path(mySettings->workDir, "newDiffersList.NoTrim.txt");
    stringList2File(newDiffersListFile, newDiffersList);
    printfWriteMsg("Wrote NON-TRIMMED new differs to 'newDiffersList.NoTrim.txt' file.", stdout);

    char  *string;
    ELEM  *elem;

    while(DL_SIZE(newDiffersList) > (MAX_OSC_DOWNLOAD * 2)){
      removeDL(newDiffersList, DL_TAIL(newDiffersList), (void **) &string);
    }

    printfWriteMsg("Trimmed differs list is below:", stdout);

    fprintStringList(fLogPtr, newDiffersList);
    if(fVerbose)
      fprintStringList(NULL, newDiffersList);

    /* adjust latest sequence -
     * tail element has remote path suffix to "state.txt" file **/
    elem = DL_TAIL(newDiffersList);
    string = (char *) DL_DATA(elem);

    result = myDownload(string, mySettings->latestStateFile);
    if(result != ztSuccess){
      printfWriteMsg("Error failed myDownload() function 4 adjusted latest state.txt", stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }

    /* update latestStateInfo TODO: check result **/
    memset(latestStateInfo, 0, sizeof(STATE_INFO));
    free(latestStateInfo);

    latestStateInfo = initialStateInfo();

    stateFile2StateInfo(latestStateInfo, mySettings->latestStateFile);

    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Replaced latest state.txt file using remote suffix: <%s>", string);
    printfWriteMsg(logBuffer, stdout);

  }

  /* compare completedList with newDiffersList to verify download **/

  completedList = initialStringList();
  if(!completedList){
    printfWriteMsg("Error failed initialStringList().", stderr);

    value2Return = ztMemoryAllocate;
    goto EXIT_CLEAN;
  }

  result = downloadFiles(completedList, newDiffersList, mySettings);
  if(result != ztSuccess){
    printfWriteMsg("Error failed downloadFiles() function.\n", stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }

  if(mySettings->noNewDiffers == FALSE){

    result = writeNewerFiles(mySettings->newDiffersFile, completedList);
    memset(logBuffer, 0, sizeof(logBuffer));
    if(result != ztSuccess){
      sprintf(logBuffer, "Error failed writeNewerFiles() function.");
      printfWriteMsg(logBuffer, stderr);

      value2Return = result;
      goto EXIT_CLEAN;
    }

    sprintf(logBuffer, "Appended <%d> filenames to new differs file: <%s>",
	    DL_SIZE(completedList), mySettings->newDiffersFile);
    printfWriteMsg(logBuffer, stdout);
  }

  if(DL_SIZE(newDiffersList) != DL_SIZE(completedList)){

    memset(logBuffer, 0, sizeof(logBuffer));
    sprintf(logBuffer, "Error number of downloaded files does not equal new differs files.\n"
	    "newDiffersList size is: <%d>, completedList size is: <%d>.",
	    DL_SIZE(newDiffersList), DL_SIZE(completedList));

    printfWriteMsg(logBuffer, stderr);

    /* write completed list to disk **/
    char   *completedListFile;

    completedListFile = appendName2Path(mySettings->workDir, "completedList.txt");
    stringList2File(completedListFile, completedList);


    value2Return = ztUnknownError;
    goto EXIT_CLEAN;

  }

  /* USE renameFile() :::: to be done AFTER we complete & verify download **/
  result = renameFile(mySettings->latestStateFile, mySettings->prevStateFile);
  if(result != ztSuccess){
    printfWriteMsg("Error failed renameFile() function.", stderr);

    value2Return = result;
    goto EXIT_CLEAN;
  }

  /* we used "start_id" file name in old "getdiff" version;
   * consider AND reconsider .... **/
  writeStartID(latestStateInfo->sequenceNumber, mySettings->prevSeqFile);

  memset(logBuffer, 0, sizeof(logBuffer));
  sprintf(logBuffer, "Successfully downloaded <%d> files; differs & their corresponding state.txt files.", DL_SIZE(completedList));

  printfWriteMsg(logBuffer, stdout);

 EXIT_CLEAN:

  if(downloadHandle)
    easyCleanup(downloadHandle);

  if(host)
    curl_free(host);

  if(path)
    curl_free(path);

  if(curlParseHandle)
    urlCleanup(curlParseHandle);

  closeCurlSession();

  if(fLogPtr){
    /* write "DONE" footer to log file **/
    logMessage(fLogPtr, "DONE");
    fclose(fLogPtr);
  }

  if(childrenList)
    zapStringList((void **) &childrenList);

  if(newDiffersList)
    zapStringList((void **) &newDiffersList);

  if(completedList)
    zapStringList((void **) &completedList);

  return value2Return;

} /* END main() **/

/* logMessage(): writes 'msg' parameter to the open file with 'to' file pointer.
 * if 'msg' points to string "START", start header is written, if it points to
 * "DONE" tail footer is written, otherwise 'msg' is appended to current time
 * then written to 'to' file.
 *
 * It is an error if 'msg' is empty (string length == 0) or longer than PATH_MAX.
 *
 *****************************************************************************/

int logMessage(FILE *to, char *msg){

  char   *startTemplate =
    "+++++++++++++++++++++++++++++++++++ STARTING +++++++++++++++++++++++++++++++++\n"
    "%s [%d]: %s started. (Version: %s)\n";

  char   *doneTemplate =
    "%s [%d]: %s is done.\n"
    "===================================== DONE ===================================\n\n";

  ASSERTARGS (to && msg);

  if(strlen(msg) == 0){
    fprintf(stderr,"logMessage(): Error 'msg' parameter is empty.\n");
    return ztInvalidArg;
  }

  if(strlen(msg) > PATH_MAX){
    fprintf(stderr,"logMessage(): Error 'msg' parameter is longer than PATH_MAX: <%d>.\n", PATH_MAX);
    return ztInvalidArg;
  }

  char   *timestamp = NULL;
  pid_t   myPID;

  char   tmpBuf[PATH_MAX] = {0};

  timestamp = formatMsgHeadTime(); //vsprintf()
  if(! timestamp){
    fprintf(stderr,"logMessage(): Error failed formatMsgHeadTime().\n");
    return ztMemoryAllocate;
  }

  myPID = getpid();

  if(strcmp(msg, "START") == 0){

    sprintf(tmpBuf, startTemplate, timestamp, (int) myPID, progName, VERSION);
    fprintf(to, tmpBuf);
  }

  else if(strcmp(msg, "DONE") == 0){

    sprintf(tmpBuf, doneTemplate, timestamp, (int) myPID, progName);
    fprintf(to, tmpBuf);
  }

  else{

    fprintf (to, "%s [%d]: %s\n", timestamp, (int) myPID, msg);

  }

  fflush(to);

  /* TODO: check fprintf() return value **/

  return ztSuccess;

} /* END logMessage() **/


/* isOkaySource(): checks source string using CURLU handle.
 * assumes handle was initialed with FULL URL source string.
 *
 * FULL URL string has set parts: scheme, host and path.
 *
 **************************************************************/
int isOkaySource(CURLU *curlSourceHandle){

  CURLUcode   curluCode;
  char   *host = NULL;
  char   *path = NULL;
  char   *scheme = NULL;

  ASSERTARGS(curlSourceHandle);

  curluCode = curl_url_get(curlSourceHandle, CURLUPART_SCHEME, &scheme, 0);
  if (curluCode != CURLUE_OK) {
    fprintf(stderr, "%s: Error failed curl_url_get() for scheme part.\n"
	    "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));
    return curluCode;
  }

  if(strcasecmp(scheme, "https") != 0){
    fprintf(stderr, "isOkaySource(): Error invalid 'scheme' part in source handle. "
	    "Scheme: <%s>\n", scheme);
    return ztInvalidArg;
  }


  curluCode = curl_url_get(curlSourceHandle, CURLUPART_HOST, &host, 0);
  if (curluCode != CURLUE_OK) {
    fprintf(stderr, "%s: Error failed curl_url_get() for server part.\n"
	    "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));
    return curluCode;
  }

  if((strcasecmp(host, "planet.openstreetmap.org") != 0) &&
     (strcasecmp(host, "planet.osm.org") != 0) &&
     (strcasecmp(host, "osm-internal.download.geofabrik.de") != 0) &&
     (strcasecmp(host, "download.geofabrik.de") != 0) ){

    fprintf(stderr, "isOkaySource(): Error server <%s> is not supported;\n"
	    "invalid 'host / server' part in source argument.\n"
	    "Double check \"SERVER\" part spelling first.\n\n", host);
    return ztInvalidArg;
  }

  curluCode = curl_url_get(curlSourceHandle, CURLUPART_PATH, &path, 0);
  if ( curluCode != CURLUE_OK ) {
    fprintf(stderr, "%s: Error failed curl_url_get() for path part.\n"
	    "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));
    return curluCode;
  }

  char   *lastEntry = NULL;
  char   *strUpdates = "-updates";

  lastEntry = lastOfPath(path);
  if( ! lastEntry ){
    fprintf(stderr, "isOkaySource(): Error failed lastOfPath() function.\n");
    return ztMemoryAllocate;
  }

  if((strcasecmp(lastEntry, "minute") != 0) &&
     (strcasecmp(lastEntry, "hour") != 0) &&
     (strcasecmp(lastEntry, "day") != 0) &&
     (strcmp(lastEntry + (strlen(lastEntry) - strlen(strUpdates)), strUpdates) != 0) ){
    
    /* lastEntry must end with the string in strUpdates **/

    fprintf(stderr, "isOkaySource(): Error invalid 'path' part in source string.\n"
	    "Path part: <%s>\n", path);
    fprintf(stderr, "isOkaySource(): Error failed lastEntry test.\n"
	    "Valid lastEntry in path is one of:\n"
	    " minute\n"
	    " hour\n"
	    " day\n"
	    " or last entry must end with \"-updates\"\n\n"
	    "Please double check your spelling.\n\n");

    return ztInvalidArg;
  }

  if(host)
    curl_free(host);

  if(path)
    curl_free(path);

  if(scheme)
    curl_free(scheme);

  return ztSuccess;

} /* END isOkaySource() **/

void printSettings(FILE *toFile, SETTINGS *settings){

  FILE *stream;

  ASSERTARGS(settings);

  if (toFile == NULL)

    stream = stdout;

  else

    stream = toFile;


  fprintf(stream, "printSettings() : Printing <%s> current settings:\n\n", progName);

  if (settings->usr)
    fprintf(stream, "OSM USER is: <%s>\n", settings->usr);
  else
    fprintf(stream, "OSM USER is NOT set.\n");

  if (settings->pswd)
    fprintf(stream, "Password is: <%s>\n", "xxxxxxxxxx");
  else
    fprintf(stream, "Password is NOT set.\n");

  if (settings->confFile)
    fprintf(stream, "Configuration File is: <%s>\n", settings->confFile);
  else
    fprintf(stream, "Configuration File is NOT set.\n");

  if (settings->source)
    fprintf(stream, "Source URL is: <%s>\n", settings->source);
  else
    fprintf(stream, "Source is NOT set.\n");

  if (settings->rootWD)
    fprintf(stream, "Root for Work directory is: <%s>\n", settings->rootWD);
  else
    fprintf(stream, "Root for Work directory is NOT set.\n");

  if (settings->workDir)
    fprintf(stream, "Work directory is: <%s>\n", settings->workDir);
  else
    fprintf(stream, "Work directory is NOT set.\n");

  if (settings->diffDir)
    fprintf(stream, "differ directory is: <%s>\n", settings->diffDir);
  else
    fprintf(stream, "differ directory is NOT set.\n");

  if (settings->startNumber)
    fprintf(stream, "Begin is: <%s>\n", settings->startNumber);
  else
    fprintf(stream, "Begin is NOT set.\n");

  if (settings->cookieFile)
    fprintf(stream, "Cookie File is: <%s>\n", settings->cookieFile);
  else
    fprintf(stream, "Cookie File is NOT set.\n");

  if (settings->scriptFile)
    fprintf(stream, "Script File is: <%s>\n", settings->scriptFile);
  else
    fprintf(stream, "Script File is NOT set.\n");

  if (settings->jsonFile)
    fprintf(stream, "JSON File is: <%s>\n", settings->jsonFile);
  else
    fprintf(stream, "JSON File is NOT set.\n");

  if (settings->logFile)
    fprintf(stream, "LOG File is: <%s>\n", settings->logFile);
  else
    fprintf(stream, "LOG File is NOT set.\n");

  if (settings->htmlFile)
    fprintf(stream, "HTML File is: <%s>\n", settings->htmlFile);
  else
    fprintf(stream, "HTML File is NOT set.\n");

  if (settings->verbose)
    fprintf(stream, "Verbose is on: <%d>\n", settings->verbose);
  else
    fprintf(stream, "Verbose is NOT set.\n");

  if (settings->noNewDiffers)
    fprintf(stream, "No New Differs is TRUE\n");
  else
    fprintf(stream, "No New Differs is FALSE.\n");

  if (settings->textOnly)
    fprintf(stream, "text only flag is: ON\n");
  else
    fprintf(stream, "text only flag is NOT set.\n");

  fprintf (stream, "\n");

  return;

} /* END printSettings() **/

/* appendName2Path(): appends the string 'name' to 'path' parameter.
 * function allocates memory and returns new string.
 * function terminates program on errors; if result string length > PATH_MAX
 * or memory failure.
 *
 * TODO: check parameters in stupid!
 *
 ***************************************************************************/
char *appendName2Path(char const *path, char const *name){

  char    *filepath;
  char    tmpBuffer[PATH_MAX] = {0};

  ASSERTARGS(path && name);

  if(strlen(path) + strlen(name) > PATH_MAX){

    fprintf(stderr, "%s: Error paths length larger than PATH_MAX in appendName2Path() function.\n"
	    "String length of path and filename larger than PATH_MAX.\n"
	    "Parameter 'path': <%s>\n"
	    "Parameter 'filename': <%s>\n"
	    "Exiting!\n", progName, path, name);
    exit(ztFnameLong);
  }

  /* use one slash between entries **/
  if(name[0] == '/')
    name = name + 1;

  if(SLASH_ENDING(path))
    sprintf(tmpBuffer, "%s%s", path, name);
  else
    sprintf(tmpBuffer, "%s/%s", path, name);

  /* filepath = STRDUP(tmpBuffer); -- we need the error message **/
  filepath = strdup(tmpBuffer);

  if( ! filepath){
    fprintf(stderr, "%s: Error failed strdup() in appendName2Path() function.\n", progName);
    exit(ztMemoryAllocate);
  }

  return filepath;

} /* END appendName2path() **/

/* updateSettings(): configure entry values are used only if NOT set in settings -
 * from command line.
 *
 *****************************************************************/
int updateSettings (SETTINGS *settings, CONF_ENTRY confEntries[]){

  CONF_ENTRY   *mover;

  ASSERTARGS (settings && confEntries);

  /* use configure setting only when not specified on command line */
  mover= confEntries;
  while (mover->key){

    switch (mover->index){

    case 0: // USER

      if ( ! settings->usr && mover->value)

	settings->usr = STRDUP(mover->value);

      break;

    case 1: // PASSWD

      if ( ! settings->pswd && mover->value)

	settings->pswd = STRDUP (mover->value);

      break;

    case 2: // SOURCE

      if ( ! settings->source && mover->value) {

	settings->source = STRDUP (mover->value);
      }
      break;

    case 3: // DEST_DIR; root for work directory

      if ( ! settings->rootWD && mover->value){

	settings->rootWD = STRDUP (mover->value);

      }
      break;

    case 4: // TEST_SITE

      if ( ! settings->tstSrvr && mover->value){

	settings->tstSrvr = STRDUP (mover->value);
      }
      break;

    case 5: // BEGIN

      if(settings->startNumber)
	printf("updateSettings(): start number here: <%s>\n", settings->startNumber);

      if ( !settings->startNumber && mover->value){

	settings->startNumber = STRDUP (mover->value);
      }
      break;

    case 6: // VERBOSE

      if (settings->verbose == 1)

	break;

      if(mover->value){

        if ((strcasecmp(mover->value, "true") == 0) ||
	    (strcasecmp(mover->value, "on") == 0) ||
	    (strcmp(mover->value, "1") == 0))

	  settings->verbose = 1;

	/* this is wrong! we only turn verbose on --
	   else if ((strcasecmp(mover->value, "false") == 0) ||
	   (strcasecmp(mover->value, "off") == 0) ||
	   (strcmp(mover->value, "0") == 0))

	   settings->verbose = 0;
	****************************************************/
      }

      break;

    case 7: // NEWER_FILE

      if(!settings->noNewDiffers && mover->value)

	settings->noNewDiffers = 1;

      break;

    default:

      break;
    } // end switch

    mover++;

  } // end while()

  return ztSuccess;

} /* END updateSettings() **/

/* setFilenames(): sets program file names in SETTINGS structure;
 * Called AFTER workDir is set, names are appended to workDir.
 *
 * function allocates memory for file names then set SETTING members pointers,
 * function aborts program on memory allocation error.
 **************************************************************************/
int setFilenames(SETTINGS *settings){

  ASSERTARGS(settings && settings->workDir);

  settings->diffDir = appendName2Path(settings->workDir, DIFF_ENTRY);

  settings->logFile = appendName2Path(settings->workDir, LOG_NAME);

  settings->htmlFile = appendName2Path(settings->workDir, INDEX_HTML);

  settings->newDiffersFile = appendName2Path(settings->workDir, NEW_DIFFERS);

  settings->scriptFile = appendName2Path(settings->workDir, SCRIPT_FILE);

  settings->jsonFile = appendName2Path(settings->workDir, JSON_FILE);

  settings->cookieFile = appendName2Path(settings->workDir, COOKIE_FILE);

  settings->prevSeqFile = appendName2Path(settings->workDir, PREV_SEQ_FILE);

  settings->prevStateFile = appendName2Path(settings->workDir, PREV_STATE_FILE);

  settings->latestStateFile = appendName2Path(settings->workDir, LATEST_STATE_FILE);

  return ztSuccess;

} /* END setFileSysVaiables() **/

/* stateFile2StateInfo(): reads & parses 'state.txt' pointed to by filename
 * then fills STATE_INFO structure pointed to by 'stateInfo'.
 *
 * caller initials 'stateInfo'.
 *
 *************************************************************************/

int stateFile2StateInfo(STATE_INFO *stateInfo, const char *filename){

  int   result;

  ASSERTARGS(stateInfo && filename);

  /* read state.txt file into a STRING_LIST **/
  STRING_LIST   *fileStrList;

  fileStrList = initialStringList();
  if( ! fileStrList){
    fprintf(stderr, "%s: Error failed initialStringList() function.\n", progName);
    return ztMemoryAllocate;
  }

  result = file2StringList(fileStrList, filename);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed file2StringList() function.\n", progName);
    return result;
  }

  /* file2StringList() returns an error when 'filename' is empty,
   * but check list anyway **/
  if(isStateFileList(fileStrList) != TRUE){
    fprintf(stderr, "%s: Error failed isStateFileList() test.\n", progName);
    return ztInvalidArg;
  }

  /* set character pointers for lines (strings) to parse **/
  char   *timeLine;
  char   *sequenceLine;
  char   *originalSeqLine = NULL;

  ELEM   *elem;

  char   *timeSubString = "timestamp";
  char   *sequenceSubString = "sequenceNumber";
  char   *originalSubString = "# original OSM";

  elem = findElemSubString(fileStrList, timeSubString);
  if( ! elem){
    fprintf(stderr, "%s: Error failed findElemSubString() for 'timestamp'.\n", progName);
    return ztUnknownError;
  }

  timeLine = (char *) DL_DATA(elem);
  if( ! timeLine ){
    fprintf(stderr, "%s: Error failed to retrieve timeLine string.\n", progName);
    return ztUnknownError;
  }

  elem = findElemSubString(fileStrList, sequenceSubString);
  if( ! elem){
    fprintf(stderr, "%s: Error failed findElemSubString() for 'sequenceNumber'.\n", progName);
    return ztUnknownError;
  }

  sequenceLine = (char *) DL_DATA(elem);
  if( ! sequenceLine ){
    fprintf(stderr, "%s: Error failed to retrieve sequenceLine string.\n", progName);
    return ztUnknownError;
  }

  elem = findElemSubString(fileStrList, originalSubString);
  if(elem){
    originalSeqLine = (char *) DL_DATA(elem);
    if( ! originalSeqLine ){
      fprintf(stderr, "%s: Error failed to retrieve originalSeqLine string.\n", progName);
      return ztUnknownError;
    }
  }

  result = parseTimestampLine(&(stateInfo->timestampTM), timeLine);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed parseStateTime() function.\n", progName);
    return result;
  }

  /* convert tm structure to time value;
   * storing result in timeValue member
   * check returned value from makeTimeGMT()
   ******************************************/

  stateInfo->timeValue = makeTimeGMT(&(stateInfo->timestampTM));
  if(stateInfo->timeValue == -1){
    fprintf(stderr, "%s: Error failed makeTimeGMT() function.\n", progName);
    return ztParseError;
  }

  char   *sequenceString;

  result = parseSequenceLine(&sequenceString, sequenceLine);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed parseSequenceLine() function.\n", progName);
    return result;
  }

  stateInfo->sequenceNumber = STRDUP(sequenceString);

  result = seq2PathParts(stateInfo->pathParts, stateInfo->sequenceNumber);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed seq2PathParts() function.\n", progName);
    return result;
  }
  /*
    printf("stateFile2StateInfo(): Time string is: <%s>\n", timeLine);

    printf("stateFile2StateInfo(): time value in stateInfo is: <%d>\n", (int)stateInfo->timeValue);

    printf("stateFile2StateInfo(): Sequence Number is: <%s>\n", stateInfo->sequenceNumber);

    printf("stateFile2StateInfo(): path is: <%s>\n", stateInfo->pathParts->path);
  */
  return ztSuccess;

} /* END stateFile2StateInfo() **/

/* isStateFileList():
 *  - must be a STRING_LT
 *  - must have at least 2 lines
 *  - one line must start with "timestamp" substring
 *  - one line must start with "sequenceNumber" substring
 *
 * **************************************************/
int isStateFileList(STRING_LIST *list){

  ASSERTARGS(list);

  if(DL_SIZE(list) < 2)

    return FALSE;

  if( ! TYPE_STRING_LIST(list) )

    return FALSE;

  ELEM   *elem;
  char   *timeSubString = "timestamp";
  char   *sequenceSubString = "sequenceNumber";
  char   *line;

  elem = findElemSubString(list, timeSubString);
  if( ! elem )

    return FALSE;

  line = (char *) DL_DATA(elem);

  /* 'timestamp' must be at line beginning **/
  if(line != strstr(line, timeSubString) )

    return FALSE;

  elem = findElemSubString(list, sequenceSubString);
  if( ! elem )

    return FALSE;

  line = (char *) DL_DATA(elem);

  if(line != strstr(line, sequenceSubString) )

    return FALSE;


  return TRUE;

} /* END isStateFileList() **/

/* parseTimestampLine(): parses time string in timestamp line
 *
 *  timestamp=2023-06-04T20\:22\:01Z
 *
 * timeString parameter is a pointer to the line above.
 *
 * I am aware of strptime() function.
 *
 ************************************************************/
int parseTimestampLine(struct tm *tmStruct, char *timeString){

  ASSERTARGS(tmStruct && timeString);

  char   *beginning = "timestamp=";
  char   *myTimeString;
  char   *allowed = "0123456789:TZ-\134"; /* 134 is Octal for forward slash **/

  /* timeString must start with 'timestamp=' **/
  if(timeString != strstr(timeString, beginning)){
    fprintf(stderr, "parseStateTime(): Error 'timeString' parameter "
	    "does not start with 'timestamp='.\n");
    return ztInvalidArg;
  }

  /* copy DATE/TIME part only into myTimeString;
   * starting after 'timestamp=' **/
  myTimeString = STRDUP(timeString + strlen(beginning));

  /* check for allowed characters **/
  if(strspn(myTimeString, allowed) != strlen(myTimeString)){
    fprintf(stderr, "%s: Error parseStateTime() parameter 'timeString' has "
	    "disallowed character.\n", progName);
    return ztDisallowedChar;
  }

  char  *yearTkn, *monthTkn, *dayTkn,
    *hourTkn, *minuteTkn, *secondTkn;

  int  year, month, day,
    hour, minute, second;

  char  *endPtr;

  char  *delimiter = "-T:Z\134";

  yearTkn = strtok(myTimeString, delimiter);
  if(!yearTkn){
    fprintf(stderr, "%s: Error failed to extract yearTkn.\n", progName);
    return ztParseError;
  }

  monthTkn = strtok(NULL, delimiter);
  if(!monthTkn){
    fprintf(stderr, "%s: Error failed to extract monthTkn.\n", progName);
    return ztParseError;
  }

  dayTkn = strtok(NULL, delimiter);
  if(!dayTkn){
    fprintf(stderr, "%s: Error failed to extract dayTkn.\n", progName);
    return ztParseError;
  }

  hourTkn = strtok(NULL, delimiter);
  if(!hourTkn){
    fprintf(stderr, "%s: Error failed to extract hourTkn.\n", progName);
    return ztParseError;
  }

  minuteTkn = strtok(NULL, delimiter);
  if(!minuteTkn){
    fprintf(stderr, "%s: Error failed to extract minuteTkn.\n", progName);
    return ztParseError;
  }

  secondTkn = strtok(NULL, delimiter);
  if(!secondTkn){
    fprintf(stderr, "%s: Error failed to extract secondTkn.\n", progName);
    return ztParseError;
  }

  year = (int) strtol(yearTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'yearTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, yearTkn);
    return ztParseError;
  }

  month = (int) strtol(monthTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'monthTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, monthTkn);
    return ztParseError;
  }

  day = (int) strtol(dayTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'dayTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, dayTkn);
    return ztParseError;
  }

  hour = (int) strtol(hourTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'hourTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, hourTkn);
    return ztParseError;
  }

  minute = (int) strtol(minuteTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'minuteTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, minuteTkn);
    return ztParseError;
  }

  second = (int) strtol(secondTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'secondTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, secondTkn);
    return ztParseError;
  }

  /* fill members in struct tm **/
  tmStruct->tm_year = year - 1900;

  tmStruct->tm_mon = month - 1; /* month is zero based **/

  tmStruct->tm_mday = day;

  tmStruct->tm_hour = hour;

  tmStruct->tm_min = minute;

  tmStruct->tm_sec = second;

  return ztSuccess;

} /* END parseStateTime() **/

/* parseSequenceLine(): extracts sequence number from sequence line.
 *
 * function allocates memory and sets sequenceSting pointer.
 *
 *
 ***************************************************************************/

int parseSequenceLine(char **sequenceString, const char *line){

  ASSERTARGS(sequenceString && line);

  /* get sequence number **/
  char   *seqString;
  char   *digits = "0123456789";
  char   *mySeqStr;

  seqString = strchr(line, '=') + 1;
  if(! seqString){
    fprintf(stderr, "%s: Error failed to extract 'seqString'.\n", progName);
    return ztParseError;
  }

  mySeqStr = STRDUP(seqString);

  removeSpaces(&mySeqStr); /* just in case **/

  /* all characters must be digits **/
  if(strspn(mySeqStr, digits) != strlen(mySeqStr)){
    fprintf(stderr, "%s: Error sequence string has non-digit character.\n", progName);
    return ztParseError;
  }

  /* it can not be more than 9 characters long **/
  if(strlen(mySeqStr) > 9){
    fprintf(stderr, "%s: Error 'sequence string' has more than 9 digits.\n", progName);
    return ztParseError;
  }

  /* set sequenceString destination pointer **/
  *sequenceString = STRDUP(mySeqStr);

  return ztSuccess;

} /* END parseSequenceLine() **/


int seq2PathParts(PATH_PARTS *pathParts, const char *sequenceStr){

  char   *path = NULL;
  char   *digits = "0123456789";

  ASSERTARGS(pathParts && sequenceStr);

  if(strlen(sequenceStr) > 9){
    fprintf(stderr, "%s: Error in seq2PathParts() parameter 'sequenceStr' "
	    "is longer than 9 characters.\n", progName);
    return ztInvalidArg;
  }

  /* sequenceStr must be all digits **/
  if(strspn(sequenceStr, digits) != strlen(sequenceStr)){
    fprintf(stderr, "%s: Error in seq2PathParts() parameter 'sequenceStr' "
	    "has non-digit character.\n", progName);
    return ztInvalidArg;
  }

  /* to format with leading character using printf(), variable must be integer type **/
  int    seqNum;
  char   *endPtr;

  seqNum = (int) strtol(sequenceStr, &endPtr, 10);
  if( *endPtr != '\0'){
    fprintf(stderr, "%s: Error in seq2PathParts() failed strtol() for sequenceStr.\n", progName);
    free(path);
    path = NULL;
    return ztInvalidArg;
  }

  /* make a string from seqNum with leading zeros **/
  char   tmpBuf[10] = {0};

  sprintf(tmpBuf, "%09d", seqNum);

  memset(pathParts, 0, sizeof(PATH_PARTS));

  /* make parentEntry, childEntry & file members - no slashes in any of those **/
  strncpy(pathParts->parentEntry, tmpBuf, 3);

  strncpy(pathParts->childEntry, tmpBuf + 3, 3);

  strncpy(pathParts->file, tmpBuf + 6, 3);

  /* make path, parentPath & childPath - ALL paths start with slash.
   * parentPath & childPath both end with slash - both are directories. **/

  sprintf(pathParts->path, "/%s/%s/%s",
	  pathParts->parentEntry, pathParts->childEntry, pathParts->file);

  sprintf(pathParts->parentPath, "/%s/", pathParts->parentEntry);

  sprintf(pathParts->childPath, "/%s/%s/", pathParts->parentEntry, pathParts->childEntry);

  return ztSuccess;

} /* END seq2PathParts() **/

void printPathParts(PATH_PARTS *pp){

  ASSERTARGS(pp);

  printf("PATH_PARTS members are:\n");

  printf(" path is: <%s>\n", pp->path);

  printf(" parentPath is: <%s>\n", pp->parentPath);

  printf(" childPath is: <%s>\n", pp->childPath);

  printf(" parentEntry is: <%s>\n", pp->parentEntry);

  printf(" childEntry is: <%s>\n", pp->childEntry);

  printf(" file is: <%s>\n", pp->file);

  return;

} /* END printPathParts() **/

/* readStartID() :
 *
 */

int readStartID(char **idString, char *filename){

  FILE  *fPtr;
  char  line[256] = {0};
  int   count = 0;
  char  *str;

  ASSERTARGS (idString && filename);

  *idString = NULL;

  errno = 0;
  fPtr = fopen(filename, "r");
  if(!fPtr){
    fprintf(stderr, "%s: Error failed to open file <%s> in readStartID().\n",
            progName, filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  while(fgets(line, 256, fPtr)){

    count++;
    if (count > 1) /* read one line only **/
      break;
  }

  fclose(fPtr);

  if(count > 1)

    return ztMalformedFile;


  if (line[strlen(line) - 1] == '\n') /* remove linefeed */

    line[strlen(line) -1] = '\0';

  str = STRDUP(line);

  removeSpaces(&str);

  if(isGoodSequenceString(str) != TRUE){
    fprintf (stderr, "%s: Error invalid sequence string: <%s> in start file <%s>\n",
	     progName, str, filename);
    return ztMalformedFile;
  }

  *idString = STRDUP(str);

  free(str);

  return ztSuccess;

} /* END readStartID() **/

int writeStartID (char *idStr, char *filename){

  FILE   *fPtr;

  ASSERTARGS (idStr && filename);

  if(isGoodSequenceString(idStr) != TRUE){
    fprintf (stderr, "%s: Error invalid sequence string in parameter 'idStr'.\n", progName);
    return ztInvalidArg;
  }

  errno = 0;
  fPtr = fopen(filename, "w");
  if (!fPtr){
    fprintf (stderr, "%s: Error failed fopen() for file! <%s>\n", progName, filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  fprintf(fPtr, "%s\n", idStr); /* we write linefeed **/

  fclose(fPtr);

  return ztSuccess;

} /* END writeStartID() **/

int isGoodSequenceString(const char *string){

  char   *digits = "1234567890";

  ASSERTARGS(string);

  if((strlen(string) > 9) || (strlen(string) < 4))

    return FALSE;

  if(strspn(string, digits) != strlen(string))

    return FALSE;

  if(string[0] == '0')

    return FALSE;

  return TRUE;

} /* END isGoodSequenceString() **/

/* myDownload():
 * appends 'remotePathSuffix' to current 'pathPrefix' in curl parse handle.
 * 'remotePathSuffix' can be null - nothing to append.
 * 'remotePathSuffix' is the path part from the sequence number, might include
 * change file name and state.txt file name.
 *
 * NOTE: FIXME do not allow null for remotePathSuffix!!!
 *
 * original 'pathPrefix' is restored before exit (return).
 *
 */

int myDownload(char *remotePathSuffix, char *localFile){

  int   result;

  CURLUcode   curluResult; /* returned type by curl_url_get() & curl_url_set() **/
  char        *pathPrefix;
  char        newPath[PATH_MAX] = {0};

  char        *currentSourceURL;

  ASSERTARGS(localFile);

  result = isGoodFilename(localFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isGoodFilename() for 'localFile' parameter in myDownload(): <%s>.\n",
	    progName, localFile);
    return result;
  }

  /* ensure that "curlParseHandle" is intact **/
  currentSourceURL = getPrefixCURLU(curlParseHandle);

  if(strcmp(sourceURL, currentSourceURL) != 0){
    fprintf(stderr, "%s: Error fatal NOT same strings in sourceURL: <%s> and currentSourceURL: <%s>.\n",
	    progName, sourceURL, currentSourceURL);
    return ztFatalError;
  }

  /* append remote path suffix to current path in parse handle **/
  if(remotePathSuffix){

    char   *allowed = "0123456789.acegostxz\057"; /* 057 octal for forward slash, letters
						     are from: state.txt & .osc.gz **/

    if(strspn(remotePathSuffix, allowed) != strlen(remotePathSuffix)){
      fprintf(stderr, "%s: Error 'remotePathSuffix' parameter has disallowed character.\n", progName);
      return ztInvalidArg;
    }

    curluResult = curl_url_get(curlParseHandle, CURLUPART_PATH, &pathPrefix, 0);
    if(curluResult != CURLUE_OK ) {
      fprintf(stderr, "%s: Error failed curl_url_get() for path part.\n"
	      "Curl error message: <%s>\n", progName,
	      curl_url_strerror(curluResult));
      return ztFailedLibCall;
    }

    /* use ONLY ONE slash between parts **/

    if(remotePathSuffix[0] == '\057')

      remotePathSuffix++;

    if(SLASH_ENDING(pathPrefix))
      sprintf(newPath, "%s%s", pathPrefix, remotePathSuffix);
    else
      sprintf(newPath, "%s/%s", pathPrefix, remotePathSuffix);


    curluResult = curl_url_set (curlParseHandle, CURLUPART_PATH, newPath, 0);
    if(curluResult != CURLUE_OK){
      fprintf(stderr, "%s: Error failed curl_url_set() for new path.\n", progName);
      return ztFailedLibCall;
    }
  }

  result = download2FileRetry(localFile, downloadHandle, curlParseHandle);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed to download file: <%s>.\n"
	    " Function failed for: <%s>\n",progName, localFile, ztCode2Msg(result));

    /* new error code; needs better handling. FIXME 12/8/2023 **/
    if(result == ztNetConnFailed)
      fprintf(stderr, "download failed for lost established connection; check cables please.\n");

    if(remotePathSuffix) /* restore pathPrefix **/
      curl_url_set (curlParseHandle, CURLUPART_PATH, pathPrefix, 0);

    return result;
  }

  /* restore pathPrefix before return **/
  if(remotePathSuffix)
    curl_url_set (curlParseHandle, CURLUPART_PATH, pathPrefix, 0);

  return ztSuccess;

} /* END myDownload() **/

/* getHeader() and getListHeaders() are not used here
 * TODO: could be used sometime; move to curlfn.c
 ****************************************************/

int getHeader(const char *tofile, const char *pathSuffix){

  int         result;
  CURLcode    cResult;
  CURLUcode   curluResult; /* returned type by curl_url_get() & curl_url_set() **/
  char        *pathPrefix;
  char        newPath[PATH_MAX] = {0};

  ASSERTARGS(tofile && pathSuffix);

  result = isGoodFilename(tofile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isGoodFilename() for 'localFile' parameter in getHeader(): <%s>.\n",
	    progName, tofile);
    return result;
  }

  char   *allowed = "0123456789.acegostxz\057"; /* 057 octal for forward slash, letters
						   are from: state.txt & .osc.gz **/

  if(strspn(pathSuffix, allowed) != strlen(pathSuffix)){
    fprintf(stderr, "%s: Error 'remotePathSuffix' parameter has disallowed character.\n", progName);
    return ztInvalidArg;
  }

  curluResult = curl_url_get(curlParseHandle, CURLUPART_PATH, &pathPrefix, 0);
  if(curluResult != CURLUE_OK ) {
    fprintf(stderr, "%s: Error failed curl_url_get() for path part.\n"
	    "Curl error message: <%s>\n", progName,
	    curl_url_strerror(curluResult));
    return ztFailedLibCall;
  }

  /* use ONLY ONE slash between parts **/

  if(pathSuffix[0] == '\057')

    pathSuffix++;

  if(SLASH_ENDING(pathPrefix))
    sprintf(newPath, "%s%s", pathPrefix, pathSuffix);
  else
    sprintf(newPath, "%s/%s", pathPrefix, pathSuffix);


  curluResult = curl_url_set (curlParseHandle, CURLUPART_PATH, newPath, 0);
  if(curluResult != CURLUE_OK){
    fprintf(stderr, "%s: Error failed curl_url_set() for new path.\n", progName);
    return ztFailedLibCall;
  }

  /* no need to save header!
   * curl_easy_getinfo(.. CURLINFO_CONTENT_LENGTH_DOWNLOAD_T ..)
   * gets the same number.
   *
   * find a way to save header on error.
   *
   *****************************************************************/

  FILE   *headerFile;

  headerFile = fopen(tofile, "w");
  if(!headerFile){
    fprintf(stderr, "%s: Error failed fopen() function in getHeader().\n", progName);
    return ztOpenFileError;
  }

  curl_easy_setopt(downloadHandle, CURLOPT_NOBODY, 1L);

  curl_easy_setopt(downloadHandle, CURLOPT_HEADERDATA, headerFile);

  /* get it **/
  cResult = curl_easy_perform(downloadHandle);

  if(!cResult) {
    /* check the size */
    curl_off_t cl;
    cResult = curl_easy_getinfo(downloadHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    if(!cResult) {
      printf("Download size: %" CURL_FORMAT_CURL_OFF_T "\n", cl);

      char buf[PATH_MAX * 2] = {0};
      sprintf(buf, "Content Length from GET_INFO for file: <%s> is <%ld>", newPath, (long) cl);
      printfWriteMsg(buf, stdout);

    }
  }


  /* restore ALL to defaults pathPrefix, headerdata & method before returning **/
  curl_url_set (curlParseHandle, CURLUPART_PATH, pathPrefix, 0);

  curl_easy_setopt(downloadHandle, CURLOPT_HEADERDATA, NULL);

  curl_easy_setopt(downloadHandle, CURLOPT_HTTPGET, 1L);


  return ztSuccess;

} /* END getHeader() **/

int getListHeaders(STRING_LIST *list, SETTINGS *settings){

  /** newDiffersList: each line is in an element

      /005/637/617.osc.gz
      /005/637/617.state.txt
      /005/637/618.osc.gz
      /005/637/618.state.txt
      /005/637/619.osc.gz
      /005/637/619.state.txt

  **/

  ASSERTARGS(list && settings);

  char  *headerTag = "header";
  char  *file;
  char  localHeader[PATH_MAX];
  char  *suffix;

  ELEM  *elem;

  elem = DL_HEAD(list);

  while(elem){

    suffix = (char *)DL_DATA(elem);

    /* skip state.txt file, get header for change file only **/
    if(strstr(suffix, "state")){
      elem = DL_NEXT(elem);
      continue;
    }

    file = appendName2Path(settings->diffDir, suffix);

    memset(localHeader, 0, sizeof(localHeader));
    sprintf(localHeader, "%s.%s", file, headerTag);

    getHeader(localHeader, suffix);

    elem = DL_NEXT(elem);
  }

  return ztSuccess;

}


STATE_INFO *initialStateInfo(){

  STATE_INFO   *newSI;

  newSI = (STATE_INFO *)malloc(sizeof(STATE_INFO));
  if(!newSI){
    fprintf(stderr, "%s: Error failed memory allocation in initialStateInfo().\n", progName);
    return newSI;
  }
  memset(newSI, 0, sizeof(STATE_INFO));

  newSI->pathParts = (PATH_PARTS *)malloc(sizeof(PATH_PARTS));
  if(!newSI->pathParts){
    fprintf(stderr, "%s: Error failed memory allocation in initialStateInfo().\n", progName);

    free(newSI);
    newSI = NULL;
    return newSI;
  }
  memset(newSI->pathParts, 0, sizeof(PATH_PARTS));

  return newSI;

} /* END initialStateInfo() **/

/* getNewDiffersList(): fills 'list' with NEWer change files and their state.txt
 * file names.
 * The remote server differs list is sorted in "ascending order"; NEWer differs
 * are those after (newer than) startPP file name member up to and including
 * latestPP file name member - which is always included.
 *
 * If includeStartFiles is set, list will include startPP file name in the list.
 *
 **********************************************************************************/

int getNewDiffersList(STRING_LIST *list, PATH_PARTS *startPP,
		      PATH_PARTS *latestPP, SETTINGS *settings, int includeStartFiles){

  int   result;

  ASSERTARGS(list && latestPP && startPP && settings);

  if(!TYPE_STRING_LIST(list)){
    fprintf(stderr, "%s: Error parameter 'list' is not of type STRING_LT.\n", progName);
    return ztInvalidArg;
  }

  if(DL_SIZE(list) != 0){
    fprintf(stderr, "%s: Error parameter 'list' is not empty list.\n", progName);
    return ztListNotEmpty;
  }

  if((includeStartFiles != 0) && (includeStartFiles != 1)){
    fprintf(stderr, "%s: Error in getNewDiffersList() invalid value for parameter 'includeStartFiles' flag.\n", progName);
    return ztInvalidArg;
  }

  /* download server file list page with path = startPP->childPath **/
  result = myDownload(startPP->childPath, settings->htmlFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myDownload() function in getNewDiffersList().\n", progName);
    return result;
  }

  STRING_LIST   *indexList; /* server file list - parsed from html page **/

  indexList = initialStringList();
  if(!indexList){
    fprintf(stderr, "%s: Error failed initialStringList().\n", progName);
    return ztMemoryAllocate;
  }

  result = parseHtmlFile(indexList, settings->htmlFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed parseHtmlFile() function in getNewDiffersList().\n", progName);
    return result;
  }

  /* parsed file okay, remove it. **/
  removeFile(settings->htmlFile);

  /* we start download list from file just AFTER last downloaded file: startPP->file.
   * files are in pairs: state.txt file & change file, we skip last downloaded file;
   * 2 elements (one for state.txt & one for change file) **/

  ELEM   *elem;
  char   *removedString;

  elem = findElemSubString(indexList, startPP->file);

  if(!elem){
    fprintf(stderr, "%s: Error failed to find first start element in getNewDiffersList().\n", progName);
    return ztFatalError; /* need error code */
  }

  if(!includeStartFiles){

    removeDL(indexList, elem, (void **) &removedString); /* remove element with first match **/

    /* find next element by file number (name) only **/
    elem = findElemSubString(indexList, startPP->file);
    if(!elem){
      fprintf(stderr, "%s: Error failed to find second start element in getNewDiffersList().\n", progName);
      return ztFatalError;
    }

    /* move element pointer AFTER last downloaded file **/
    elem = DL_NEXT(elem);

  }

  result = insertFileWithPath(list, elem, startPP->childPath);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed insertFileWithPath() function.\n", progName);
    return result;
  }

  if(strcmp(startPP->childEntry, latestPP->childEntry) != 0){

    /* latest file is in another server directory - path), get html page,
       parse it AND add files to download list with latest childPath **/

    /*
      destroyDL(indexList);  reuse list
      free(indexList);
      indexList = NULL;
    */

    zapStringList((void **) &indexList); /* reuse list **/

    indexList = initialStringList();
    if(!indexList){
      fprintf(stderr, "%s: Error failed initialStringList().\n", progName);
      return ztMemoryAllocate;
    }

    result = myDownload(latestPP->childPath, settings->htmlFile);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed myDownload() function in getNewDiffersList().\n", progName);
      return result;
    }

    result = parseHtmlFile(indexList, settings->htmlFile);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed parseHtmlFile() function in getNewDiffersList().\n", progName);
      return result;
    }

    result = insertFileWithPath(list, DL_HEAD(indexList), latestPP->childPath);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed insertFileWithPath() function.\n", progName);
      return result;
    }

  } /* end if(strcmp ...) **/

  /* ensure last file in the list is the latest one; not AFTER latest
   * this is because of time lag between getting latest from state.txt
   * and getting index page from server, latest may get updated in this time.
   * we may also get just one file from the update not both, consider this case too. **/

  char  *string;
  int   removedCount = 0;

  elem = DL_TAIL(list);
  string = (char *)DL_DATA(elem);

  while(strncmp(latestPP->path, string, (size_t) strlen(latestPP->path)) != 0){

    if(removedCount == 2){
      fprintf(stderr, "%s: Error removed 2 elements still did NOT match latest path string!\n", progName);
      return ztUnknownError;
    }

    removeDL(list, elem, (void **)&string);

    removedCount++;

    elem = DL_TAIL(list);
    string = (char *)DL_DATA(elem);
  }

  /* list MUST have an even list size **/
  if(DL_SIZE(list) && ! IS_EVEN(DL_SIZE(list))){
    fprintf(stderr, "%s: Error in getNewDiffersList(); odd download list size: <%d>\n",
	    progName, DL_SIZE(list));
    return ztUnknownError;
  }

  /* cleanup after your self **/
  if(indexList)
    zapStringList((void **) &indexList);

  return ztSuccess;

} /* END getNewDiffersList() **/

int insertFileWithPath(STRING_LIST *toList, ELEM *startElem, char *path){

  char   *filename;
  char   buffer[512];
  char   *newString;
  ELEM   *elem;
  int    result;

  ASSERTARGS(toList && startElem && path);

  elem = startElem;

  while(elem){

    filename = (char *)DL_DATA(elem);

    memset(buffer, 0, sizeof(buffer) * sizeof(char));

    sprintf(buffer, "%s%s", path, filename);

    newString = STRDUP(buffer);

    result = listInsertInOrder(toList, newString);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed ListInsertInOrder() function.\n", progName);
      return result;
    }

    elem = DL_NEXT(elem);

  }

  return ztSuccess;

} /* END insertFileWithPath() **/

int downloadFiles(STRING_LIST *completed, STRING_LIST *downloadList, SETTINGS *settings){

  int    result;
  ELEM   *elem;
  char   *filename;
  char   *pathSuffix;
  char   localFilename[PATH_MAX];

  int    iCount = 0;
  int    sleepSeconds = 0;

  ASSERTARGS(completed && downloadList && settings);

  /* completed: filename is inserted once download is complete successfully
   * should be empty. caller initials list.
   */

  elem = DL_HEAD(downloadList);
  while(elem){

    pathSuffix = (char *)DL_DATA(elem);
    filename = lastOfPath(pathSuffix);

    if(settings->textOnly && ! strstr(filename, ".state.txt")){
      elem = DL_NEXT(elem);
      continue;
    }

    iCount++;

    /* no delay for first 12 files **/
    if(iCount == 13)
      sleepSeconds = SLEEP_INTERVAL;

    /* increment sleep time after each 4 files **/
    if( ((iCount - 1) % 4) == 0 )
      sleepSeconds += SLEEP_INTERVAL;

    memset(localFilename, 0, sizeof(char) * sizeof(localFilename));
    if(SLASH_ENDING(settings->diffDir))
      sprintf(localFilename, "%s%s", settings->diffDir, pathSuffix + 1); // (pathSuffix + 1) skip FIRST slash in path
    else
      sprintf(localFilename, "%s/%s", settings->diffDir, pathSuffix + 1);

    /* wait before next download **/
    sleep(sleepSeconds);

    result = myDownload(pathSuffix, localFilename);
    if(result == ztSuccess){
      /* each list must have its own copy of data; this is
       * so zapString() does not free same pointer again. **/

      char *pathSuffixCopy;
      pathSuffixCopy = STRDUP(pathSuffix);

      insertNextDL(completed, DL_TAIL(completed), (void **) pathSuffixCopy);
    }
    else{
      fprintf(stderr, "%s: Error failed myDownload() function for localFilename: <%s>\n",
	      progName, localFilename);
      return result;
    }

    /* file sizes are checked in download2File() function **/

    elem = DL_NEXT(elem);
  }

  return ztSuccess;

} /* END downloadFiles() **/

int makeOsmDir(PATH_PARTS *startPP, PATH_PARTS *latestPP, const char *rootDir){

  int    result;
  char   buffer[PATH_MAX] = {0};

  ASSERTARGS(startPP && latestPP && rootDir);

  /* startPP & latestPP must share parent directory **/
  if(strcmp(startPP->parentEntry, latestPP->parentEntry) != 0){
    fprintf(stderr, "%s: Error parentEntry is not the same for parameters.\n", progName);
    return ztInvalidArg;
  }

  /* rootDir must exist and accessible **/
  result = isDirUsable(rootDir);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isDirUsable() for parameter 'rootDir'.\n", progName);
    return result;
  }

  if(SLASH_ENDING(rootDir))
    sprintf(buffer, "%s%s", rootDir, startPP->parentPath);
  else
    sprintf(buffer, "%s/%s", rootDir, startPP->parentPath);

  result = myMkDir(buffer);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myMkDir() function.\n", progName);
    return result;
  }

  sprintf(buffer, "%s%s", rootDir, startPP->childPath);

  result = myMkDir(buffer);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myMkDir() function.\n", progName);
    return result;
  }

  if(strcmp(startPP->childEntry, latestPP->childEntry) == 0)

    return ztSuccess;


  sprintf(buffer, "%s%s", rootDir, latestPP->childPath);

  result = myMkDir(buffer);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myMkDir() function.\n", progName);
    return result;
  }

  return ztSuccess;

} /* END makeOsmDir() **/

void printfWriteMsg(char *msg,FILE *tof){

  /* function provide final linefeed,
   * do not include THE FINAL linefeed in msg **/

  ASSERTARGS(msg && tof);

  if(tof == stderr)

    fprintf(stderr, "%s: %s\n", progName, msg);

  if(fVerbose && tof == stdout)

    fprintf(stdout, "%s: %s\n", progName, msg);

  logMessage(fLogPtr, msg);

  return;

} /* END printfWriteMsg() **/

/* writeNewerFile():
 *
 * initial string list
 *
 * if toFile exist (usable) -> read file into string list
 *
 * append completedList to string list
 *
 * write string list to file.
 *
 *
 *************************************************************************/

int writeNewerFiles(char const *toFile, STRING_LIST *list){

  ASSERTARGS(toFile && list);

  if(DL_SIZE(list) < 2){ /* size should be even number **/
    fprintf(stderr, "%s: Error list size is less than 2 in 'list' parameter.\n", progName);
    return ztInvalidArg;
  }

  STRING_LIST   *fileList;
  int           result;
  ELEM          *elem;
  char          *line;

  fileList = initialStringList();
  if(!fileList){
    fprintf(stderr, "%s: Error failed initialStringList() function.\n", progName);
    return ztMemoryAllocate;
  }


  result = isFileUsable(toFile);
  if(result == ztSuccess){
    result = file2StringList(fileList, toFile);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed file2StringList() function.\n", progName);
      return result;
    }
  }

  elem = DL_HEAD(list);
  while(elem){

    line = (char *)DL_DATA(elem);

    result = insertNextDL(fileList, DL_TAIL(fileList), (void *) line);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed inesrtNextDL() function.\n", progName);
      return result;
    }

    elem = DL_NEXT(elem);
  }

  result = stringList2File(toFile, fileList);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed stringList2File() function.\n", progName);
    return result;
  }

  return ztSuccess;

} /* END writeNewerFiles()  **/

