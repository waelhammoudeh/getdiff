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

#define REMOVE_TMP
//#undef REMOVE_TMP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

#include "debug.h"

/*global variables **/
char   *progName = NULL;
int    fVerbose = 0;
FILE   *fLogPtr = NULL;
int    fUsingPreviousID = 0;

/* curl easy handle and curl parse handle **/
static CURL   *downloadHandle = NULL;
static CURLU  *curlParseHandle = NULL;
static char   *sourceURL = NULL;
static char   *tmpDir = NULL;

int main(int argc, char *argv[]){

  /* progName is used in output / log messages. **/
  progName = lastOfPath(argv[0]);

  if(argc == 1)

    shortHelp();

  MY_SETTING mySetting;
  int        result;

  /* get settings from command line & configuration file, function will fail
   * if work directory is not accessible - among other reasons.
   * help is handled in parseCmdLine().
   ************************************************************************/
  result = getSettings(&mySetting, argc, argv);
  if(result != ztSuccess){
    fprintf (stderr, "%s: Error failed getSettings() function.\n", progName);
    return result;
  }

  /* source argument is required **/
  if(! mySetting.source){
    fprintf (stderr, "%s: Error missing required  remote 'source url' argument.\n",
             progName);
    return ztMissingArg;
  }

  /* create program directories and setup filenames.
   * Files are NOT created here; just filenames are setup.
   * Now we know where previous sequence file is to check
   * for program first run status - is start number required?
   ********************************************************/
  SKELETON myDir;
  GD_FILES myFiles;

  result = setupFilesys(&myDir, &myFiles, mySetting.rootWD);
  if(result != ztSuccess){
    fprintf (stderr, "%s: Error failed setupFilesys() function.\n", progName);
    return result;
  }

  int lockFD;

  result = getLock(&lockFD, myFiles.lockFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed to obtain lock file. Another instance is using work directory!\n", progName);
    return ztInvalidUsage;
  }

  /* Note that we pass 'lockFD' to cookie.c functions if we have to use'em below with:
   *
   *   fd2CloseFD = lockFD; // let fork()ed child release it -- cookie.c
   *
   **********************************************************************************/

  /* open log file and write start heading.
   * fLogPtr is global in this file; used by other functions. **/
  fLogPtr = initialLog(myFiles.logFile);
  if(! fLogPtr){
    fprintf(stderr, "%s: Error failed initialLog().\n", progName);
    return ztOpenFileError;
  }

  if (mySetting.verbose == 1){ /* set global 'fVerbose' & make some noise! **/
    fVerbose = 1;

    fprintf(stdout, "%s: Starting...\n", progName);
    fprintf(stdout, "%s: Got lock Okay, lockFD is: %d\n", progName, lockFD);
    fprintSetting(stdout, &mySetting);
    fprintSkeleton(stdout, &myDir);
    fprintGdFiles(stdout, &myFiles);

    fprintSetting(fLogPtr, &mySetting);
    fprintSkeleton(fLogPtr, &myDir);
    fprintGdFiles(fLogPtr, &myFiles);
  }

  /* let our curl and cookie functions use our log file **/
  curlLogtoFP = fLogPtr;
  cookieLogFP = fLogPtr;

  /* set global "sourceURL" variable:
   * convert source string to lower case - in 'sourceURL'.
   * curl URL parser is used to verify source URL.
   ******************************************************/
  result = string2Lower((char **) &sourceURL, mySetting.source);
  if(!sourceURL){
    fprintf(stderr, "%s: Error failed string2Lower() function.\n", progName);
    return result;
  }

  CURLUcode   curluResult; /* parser returned type from curl_url_get() & curl_url_set() **/
  int   useInternal;
  char  *secToken = NULL; /* needed for geofabrik internal server **/
  char  *host = NULL;
  char  *path = NULL;

  int         value2Return = ztSuccess; /* value to return at EXIT_CLEAN **/

  /* checked in EXIT_CLEAN **/
  STRING_LIST *newDiffersList = NULL;
  STRING_LIST *completedList = NULL;

  result = initialCurlSession();
  if (result != ztSuccess){
    fprintf(stderr, "%s: Error failed initialCurlSession() function.\n", progName);
    logMessage(fLogPtr, "Error failed initialCurlSession() function.");

    return result;
  }
  else
    logMessage(fLogPtr, "Initialed curl session okay.");

  /* get curl parse handle using sourceURL - in LOWER case **/
  curlParseHandle = initialURL(sourceURL);
  if (! curlParseHandle ){
    fprintf(stderr, "%s: Error failed initialURL() function.\n", progName);
    logMessage(fLogPtr,"Error failed initialURL() function.");

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }

  if(fVerbose){
    fprintf(stdout, "%s: Acquired curl parse handle with initialURL() function okay.\n", progName);
    logMessage(fLogPtr, "Acquired curl parse handle with initialURL() function okay.");
  }

  result = chkRequired(&mySetting, myFiles.previousSeqFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed chkRequired() function.\n", progName);
    logMessage(fLogPtr, "Error failed chkRequired() function.");

    value2Return = result;
    goto EXIT_CLEAN;
  }

  /* if newer files is turned off by user AND we find an old file in
   * working directory, we rename it by appending ".old~" extension
   * Note: the '~' character makes this a bad file name, which we cannot
   * write to!
   *
   ****************************************************************/

  if(mySetting.newDifferOff){

    fprintf(stdout, "%s: No new differs file is set.\n", progName);
    logMessage(fLogPtr, "No new differs file is set.");

    result = isFileUsable(myFiles.newDiffersFile);
    if(result == ztSuccess){

      char   newName[PATH_MAX] = {0};

      sprintf(newName, "%s.old~", myFiles.newDiffersFile);

      result = renameFile(myFiles.newDiffersFile, newName);
      if(result != ztSuccess){
        fprintf(stderr, "%s: Error failed renameFile() for <%s>.\n", progName, myFiles.newDiffersFile);
        logMessage(fLogPtr, "Error failed renameFile() for 'newerFiles.txt'.");

        return result;
      }
      else{
        fprintf(stdout, "%s: Renamed 'newerFiles.txt' to 'newerFiles.txt.old~'.\n", progName);
        logMessage(fLogPtr, "Renamed 'newerFiles.txt' to 'newerFiles.txt.old~'.");
      }
    }
  }

  /* remove {workDir}/previous.state.txt if found - no longer used
   * we have been writing "previous.seq" file since last version. **/
  if(isFileUsable(myFiles.prevStateFile) == ztSuccess){
	removeFile(myFiles.prevStateFile);
	fprintf(stdout, "%s: Removed <%s> file; using 'previous.seq' now.\n", progName, myFiles.prevStateFile);
	logMessage(fLogPtr, "Removed <previous.state.txt> file from work directory; using 'previous.seq' now.");
  }

  /* set tmpDir; some functions write their own temporary files **/
  tmpDir = STRDUP(myDir.tmp);

  char *diffDestPrefix; /* new differs destination on local machine;
                           Prefix: first (start part of the path)
                           Suffix: second (end part of the path) **/

  diffDestPrefix = setDiffersDirPrefix(&myDir, sourceURL);
  if(!diffDestPrefix){
    fprintf(stderr, "%s: Error failed setDiffersDirPrefix() function.\n", progName);
    logMessage(fLogPtr, "Error failed setDiffersDirPrefix() function.");
    return ztFatalError;
  }
  if(fVerbose){
    fprintf(stdout, "%s: Download destination for new differs: %s\n", progName, diffDestPrefix);
    logMessage(fLogPtr, "Download destination for new differs is below:-");
    logMessage(fLogPtr, diffDestPrefix);
  }

  /* use curl parse handle to retrieve 'host' to set useInternal flag,
   * get 'path' also, used down below in the code.
   *******************************************************************/
  curluResult = curl_url_get(curlParseHandle, CURLUPART_HOST, &host, 0);
  if (curluResult != CURLUE_OK ) {
    fprintf(stderr, "%s: Error failed curl_url_get() for 'host' part.\n", progName);
    logMessage(fLogPtr, "Error failed curl_url_get() for 'host' part.");

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }

  curluResult = curl_url_get(curlParseHandle, CURLUPART_PATH, &path, 0);
  if (curluResult != CURLUE_OK ) {
    fprintf(stderr, "%s: Error failed curl_url_get() for path part.\n", progName);
    logMessage(fLogPtr, "Error failed curl_url_get() for path part.");

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }

  if(fVerbose){
    fprintf(stdout, "%s: Retrieved 'host' and 'path' from curl parse handle okay.\n", progName);
    logMessage(fLogPtr, "Retrieved 'host' and 'path' from curl parse handle okay.");
  }

  useInternal = (strcmp(host, INTERNAL_SERVER) == 0);

  if(useInternal){ /* set cookie; login token from cookie file. see 'cookie.c' file **/

    fd2Close = lockFD; // let fork()ed child release it; child inherits fd -- cookie.c

    if(fVerbose)
      fprintf(stdout, "%s: Geofabrik Internal Server is in use, calling getLoginToken() function...\n", progName);
    logMessage(fLogPtr, "Geofabrik Internal Server is in use, Calling getLoginToken() function...");

    secToken = getLoginToken(&mySetting, &myDir);
    if(!secToken){
      fprintf(stderr, "%s: Error failed getLoginToken() function.\n", progName);
      logMessage(fLogPtr, "Error failed getLoginToken() function.");

      value2Return = ztNoCookieToken;
      goto EXIT_CLEAN;
    }

    if(fVerbose){
      fprintf(stdout, "%s: Retrieved 'login token' from cookie file okay.\n", progName);
      logMessage(fLogPtr, "Retrieved 'login token' from cookie file okay.");
    }

  } /* end if(useInternal) **/

  /* to communicate with remote server; get downloadHandle **/
  downloadHandle = initialOperation(curlParseHandle, secToken);
  if( !downloadHandle ){
    fprintf(stderr, "%s: Error failed initialOperation() function.\n", progName);
    logMessage(fLogPtr, "Error failed initialOperation() function.");

    value2Return = ztFailedLibCall;
    goto EXIT_CLEAN;
  }
  else{
    if(fVerbose){
      fprintf(stdout, "%s: Obtained curl download handle okay.\n", progName);
      logMessage(fLogPtr, "Obtained curl download handle okay.");
    }
  }

  int   firstUse;

  result = isFileUsable(myFiles.previousSeqFile);

  firstUse = (result != ztSuccess);

  if(firstUse){
    fprintf(stdout, "%s: Program first run, did not find previous sequence file in working directory.\n", progName);
    logMessage(fLogPtr, "Program first run, did not find previous sequence file in working directory.");
  }
  else{
    fprintf(stdout, "%s: Not first run for program, found previous sequence file in working directory.\n", progName);
    logMessage(fLogPtr, "Not first run for program, found previous sequence file in working directory.");
  }

  char *startSequenceNum = NULL;
  char *endSequenceNum = NULL;


  /* set start & end numbers:
   * if RANGE:
   *   start = 'begin' from setting
   *   end = 'end' from setting
   *
   * else: (not range; no 'end' argument)
   *   end = latest sequence from remote server "state.txt" file
   *  if first run:
   *    start = 'begin' from setting
   *  else: //not first run
   *    start = previous ID from local file
   *
   *****************************************************************************/

  if(mySetting.endNumber){ // user requesting RANGE (program has 'end' argument)

    startSequenceNum = mySetting.startNumber;
    endSequenceNum = mySetting.endNumber;

    fprintf(stdout, "%s: Range download requested.\n", progName);
    logMessage(fLogPtr, "Range download requested.");

    fprintf(stdout, "%s: Start sequence number: %s\n", progName, startSequenceNum);
    fprintf(stdout, "%s: End sequence number: %s\n", progName, endSequenceNum);

    logMessage(fLogPtr, "Start sequence number is below:");
    logMessage(fLogPtr, startSequenceNum);

    logMessage(fLogPtr, "End sequence number is below:");
    logMessage(fLogPtr, endSequenceNum);

    /* firstUse flag is ignored here; startSequenceNum used is 'begin' argument.
     * handle case we do nothing **/
/*
 *  allow single file download with RANGE function - not fully tested 11/17/2024
 *  if(strcmp(startSequenceNum, endSequenceNum) == 0){
      fprintf(stdout, "%s: Start sequence number equals end sequence number! Nothing to do; exiting.\n", progName);
      logMessage(fLogPtr, "Start sequence number equals end sequence number! Nothing to do; exiting.");

      value2Return = ztSuccess;
      goto EXIT_CLEAN;
    }
*/
  }
  else{ // no 'end' argument

    fprintf(stdout, "%s: Getting latest 'state.txt' file from remote ...\n", progName);
    logMessage(fLogPtr, "Getting latest 'state.txt' file from remote ...");

    endSequenceNum = fetchLatestSequence(STATE_FILE, myFiles.latestStateFile);
    /* fetch latest sequence number from remote server; it is in 'state.txt'
     * file found at program required 'source' argument with name 'state.txt'.
     * initially placed in our 'tmpDir' AND then moved to working directory
     * farther down!
     ************************************************************************/

    if(!endSequenceNum){
      fprintf(stderr, "%s: Error failed fetchLatestSequence().\n", progName);
      logMessage(fLogPtr, "Error failed fetchLatestSequence().");

      value2Return = ztUnknownError; // this is a problem!
      goto EXIT_CLEAN;
    }

    fprintf(stdout, "%s: End Sequence Number is set to sequence number from 'latest.state.txt' file; endSequenceNum: %s\n",
              progName, endSequenceNum);
    logMessage(fLogPtr, "End Sequence Number is set to sequence number from 'latest.state.txt' file; endSequenceNum is below:");
    logMessage(fLogPtr, endSequenceNum);

    if(firstUse){

      startSequenceNum = mySetting.startNumber;

      fprintf(stdout, "%s: Program first use; start sequence number is 'begin' argument: %s.\n",
                progName, startSequenceNum);
      logMessage(fLogPtr, "Program first use; start sequence number is 'begin' argument: --below.");
      logMessage(fLogPtr, startSequenceNum);

    }
    else { // not first use

      fprintf(stdout, "%s: Reading 'previous.seq' file (ID file) for start sequence number.\n", progName);
      logMessage(fLogPtr, "Reading 'previous.seq file (ID file) for start sequence number.");

      startSequenceNum = readPreviousID(myFiles.previousSeqFile);

      if(!startSequenceNum){
        fprintf(stderr, "%s: Error failed readPreviousID().\n", progName);
        logMessage(fLogPtr, "Error failed readPreviousID().");

        value2Return = ztUnknownError; // this is a problem! readPreviousID() does NOT return/set error code!
        goto EXIT_CLEAN;
      }

      fprintf(stdout, "%s: startSequenceNum is set from 'previous.seq' file: %s\n", progName, startSequenceNum);
      logMessage(fLogPtr, "startSequenceNum is set as 'previous.seq' file, value below:");
      logMessage(fLogPtr, startSequenceNum);

      fUsingPreviousID = 1;
    }

    /* handle case we do nothing;
     * test is NOT done when firstUse == TRUE. **/
    if(!firstUse && strcmp(startSequenceNum, endSequenceNum) == 0){
      fprintf(stdout, "%s: No new differs from server; latest sequence number equals previous sequence number; exiting.\n", progName);
      logMessage(fLogPtr, "No new differs from server; latest sequence number equals previous sequence number; exiting.");

      removeFile(myFiles.latestStateFile); // this should be the same file as in our work directory

      value2Return = ztSuccess;
      goto EXIT_CLEAN;
    }
  }

  fprintf(stdout, "StartSequenceNum is: <%s>\n", startSequenceNum);
  fprintf(stdout, "endSequenceNum is: <%s>\n", endSequenceNum);

  result = areNumsGoodPair(startSequenceNum, endSequenceNum);
  if(result == ztFileNotFound){
    fprintf(stderr, "%s: Error failed areNumbsGoodPair() with file not found error from remote server!\n"
            "Please note that Geofabrik does not keep differ files for very long time.\n", progName);
    logMessage(fLogPtr, "Error failed areNumbsGoodPair() with file not found error from remote server!");
    logMessage(fLogPtr, "Please note that Geofabrik does not keep differ files for very long time.");

    value2Return = result;
    goto EXIT_CLEAN;
  }
  else if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed areNumbsGoodPair(); start sequence and end sequence are not good pair.\n", progName);
    logMessage(fLogPtr, "Error failed areNumbsGoodPair(); start sequence and end sequence are not good pair.");

    value2Return = ztInvalidArg;
    goto EXIT_CLEAN;
  }
  else{
    fprintf(stdout, "%s: Validated sequence numbers okay for good numbers pair.\n", progName);
    logMessage(fLogPtr, "Validated sequence numbers okay for good numbers pair.");
  }

  PATH_PART startSeqPP, endSeqPP; // actual structures; not pointers

  result = sequence2PathPart(&startSeqPP, startSequenceNum);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed sequence2PathPart() for startSequenceNum.\n", progName);
    logMessage(fLogPtr, "Error failed sequence2PathPart() for startSequenceNum.");

    return result;
  }

  result = sequence2PathPart(&endSeqPP, endSequenceNum);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed sequence2PathPart() for endSequenceNum.\n", progName);
    logMessage(fLogPtr, "Error failed sequence2PathPart() for endSequenceNum.");

    return result;
  }

  newDiffersList = initialStringList();
  if(!newDiffersList){
    fprintf(stderr, "%s: Error failed initialStringList() function.\n", progName);
    logMessage(fLogPtr, "Error failed initialStringList() function.");

    value2Return = ztMemoryAllocate;
    goto EXIT_CLEAN;
  }

  result = getDiffersList(newDiffersList, &startSeqPP, &endSeqPP);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed getDiffersList().\n", progName);
    logMessage(fLogPtr, "Error failed getDiffersList().");

    value2Return = result;
    goto EXIT_CLEAN;
  }

  char logStr[1024] = {0};

  if(DL_SIZE(newDiffersList)){
    fprintf(stdout, "%s: Filled newDiffersList with size: %d\n", progName, DL_SIZE(newDiffersList));
    sprintf(logStr, "Filled newDiffersList with size: %d", DL_SIZE(newDiffersList));
    logMessage(fLogPtr, logStr);
  }

  fprintStringList(NULL, newDiffersList);
  fprintStringList(fLogPtr, newDiffersList);

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

    fprintf(stdout, STYLE_BOLD
            "Updating data older than 2 months is not advisable.\n"
            STYLE_NO_BOLD);

    fprintf(stdout, "%s: Warning, trimming over-sized list ...\n"
            "New differs list size is: <%d> change files; trimming to maximum allowed per run <%d>\n"
            "Please wait at least 30 minutes for a rerun to fetch the rest.\n"
            "This is so we do not overwhelm the server and to avoid sending too many requests.\n",
            progName, (DL_SIZE(newDiffersList) / 2), MAX_OSC_DOWNLOAD);

    logMessage(fLogPtr, "Trimming new differs list to 61 change files. "
               "Please wait at least 30 minutes before a rerun.");

    char  *removedString; /* pointer to data in removed element **/

    while(DL_SIZE(newDiffersList) > (MAX_OSC_DOWNLOAD * 2)){ // remove elements from tail
      removeDL(newDiffersList, DL_TAIL(newDiffersList), (void **) &removedString);
      /* remove elements from tail of list **/
    }

    fprintf(stdout, "%s: Printing TRIMMED differs list below:\n", progName);
    logMessage(fLogPtr, "Printing TRIMMED differs list below:");

    fprintStringList(NULL, newDiffersList);
    fprintStringList(fLogPtr, newDiffersList);

    // replace latestStateFile with that in trimmed list
    ELEM *elem;
    char *stateFile;

    elem = DL_TAIL(newDiffersList);
    stateFile = (char *)DL_DATA(elem);

    result = myDownload(stateFile, myFiles.latestStateFile);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed myDownload() function.\n", progName);
      logMessage(fLogPtr, "Error failed myDownload() function.\n");

      value2Return = result;
      goto EXIT_CLEAN;
    }

    fprintf(stdout, "%s: Replaced latestStateFile with that of: %s\n", progName, lastOfPath(stateFile));

    char logBuff[1024] = {0};

    sprintf(logBuff, "Replaced latestStateFile with that of: %s\n", lastOfPath(stateFile));
    logMessage(fLogPtr, logBuff);

    // update endSequenceNum and its PATH_PART structure
    endSequenceNum = stateFile2SequenceString(myFiles.latestStateFile);

    memset(&endSeqPP, 0, sizeof(PATH_PART));

    result = sequence2PathPart(&endSeqPP, endSequenceNum);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed sequence2PathPart() for endSequenceNum.\n", progName);
      logMessage(fLogPtr, "Error failed sequence2PathPart() for endSequenceNum.");

      return result;
    }
  } //end if(size big)

  /* make sure we have local directories setup for new differ files,
   * directory entries are made as needed for start & end sequence numbers.
   **********************************************************************/
  result = makeOsmDir(&startSeqPP, &endSeqPP, diffDestPrefix);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed makeOsmDir().\n", progName);
    logMessage(fLogPtr, "Error failed makeOsmDir().");

    value2Return = result;
    goto EXIT_CLEAN;
  }

  completedList = initialStringList();
  if(!completedList){
    fprintf(stderr, "%s: Error failed initialStringList().\n", progName);
    logMessage(fLogPtr, "Error failed initialStringList().");

    value2Return = ztMemoryAllocate;
    goto EXIT_CLEAN;
  }

  fprintf(stdout, "%s: Downloading <%d> files...\n\n", progName, DL_SIZE(newDiffersList));
  logMessage(fLogPtr, "Downloading files...");

  result = downloadFilesList(completedList, newDiffersList, diffDestPrefix, mySetting.textOnly);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed downloadFilesList().\n", progName);
    logMessage(fLogPtr, "Error failed downloadFilesList().");

    value2Return = result;
    goto EXIT_CLEAN;
  }

  fprintf(stdout, "%s: Download complete.\n\n", progName);
  logMessage(fLogPtr, "Download complete.");

  char *toFile;

  if(mySetting.endNumber){
    toFile = myFiles.rangeFile;

    if(strstr(sourceURL, "planet")){

      /* prepend [minute | hour | day] to path from PLANET source
       * this is done when using RANGE function and for server
       * is planet server
       * granularity
       *****************************************************/
      char *granularity = lastOfPath(sourceURL);

      result = prependGranularity(&completedList, granularity);
      if(result != ztSuccess){
        fprintf(stderr, "%s: Error failed prependGranularity() function.\n", progName);
        logMessage(fLogPtr, "Error failed prependGranularity() function.");

        value2Return = result;
        goto EXIT_CLEAN;
      }
      fprintf(stdout, "%s: Prepended granularity \"%s\" to path in completedList.\n",
    		          progName, lastOfPath(sourceURL));
      logMessage(fLogPtr, "Prepended granularity \"below\" to path in completedList.");
      logMessage(fLogPtr, lastOfPath(sourceURL));
      fflush(NULL);
    }
  }
  else if(mySetting.newDifferOff == FALSE)
    toFile = myFiles.newDiffersFile;
  else
    toFile = NULL;

  if(!mySetting.textOnly && toFile){ //no list file is written with TEXT_ONLY option!
    result = writeNewerFiles(toFile, completedList);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed writeNewerFiles().\n", progName);
      logMessage(fLogPtr, "Error failed writeNewerFiles().");

      value2Return = result;
      goto EXIT_CLEAN;
    }

    fprintf(stdout, "%s: Wrote (appended) completed list to file: %s\n", progName, toFile);
    logMessage(fLogPtr, "Wrote (appended) completed list to file below:");
    logMessage(fLogPtr, toFile);

  }

  if(!mySetting.textOnly && DL_SIZE(newDiffersList) != DL_SIZE(completedList)){

    fprintf(stderr, "%s: Error, completed list size: <%d> does NOT equal differs list size: <%d>\n",
            progName, DL_SIZE(completedList), DL_SIZE(newDiffersList));

    char logBuff[1024] = {0};
    sprintf(logBuff, "Error, completed list size: <%d> does NOT equal differs list size: <%d>\n",
            DL_SIZE(completedList), DL_SIZE(newDiffersList));
    logMessage(fLogPtr, logBuff);

    fprintf(stderr, "%s: Completed list is below:\n", progName);
    fprintStringList(stderr, completedList);

    fprintf(stderr, "%s: New Differs list is below:\n", progName);
    fprintStringList(stderr, newDiffersList);

    logMessage(fLogPtr, "Completed list is below:");
    fprintStringList(fLogPtr, completedList);

    logMessage(fLogPtr, "New Differs list is below:");
    fprintStringList(fLogPtr, newDiffersList);

    value2Return = ztUnknownError;
    goto EXIT_CLEAN;
  }

  /* no longer keeping previous state file - use previous ID file. **/
  if(!mySetting.endNumber){
    result = writeStartID(endSequenceNum, myFiles.previousSeqFile);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed writeStartID().\n", progName);
      logMessage(fLogPtr, "Error failed writeStartID().");

      value2Return = result;
      goto EXIT_CLEAN;
    }

    /* move latest.state.txt from our /tmp to working directory
     * rename() function is used to move the file. **/

    char permLocation[1024] = {0};
    if(SLASH_ENDING(myDir.workDir))
      sprintf(permLocation, "%s%s", myDir.workDir, LATEST_STATE_FILE);
    else
      sprintf(permLocation, "%s/%s", myDir.workDir, LATEST_STATE_FILE);

    result = renameFile(myFiles.latestStateFile, permLocation);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed rename() to move latest state file.\n", progName);
      logMessage(fLogPtr, "Error failed rename() to move latest state file.");

      value2Return = result;
      goto EXIT_CLEAN;
    }
  }

  fprintf(stdout, "%s: Successfully downloaded <%d> files to: %s\n Exiting normally.\n",
          progName, DL_SIZE(completedList), diffDestPrefix);
  char logBuff[1024] = {0};
  sprintf(logBuff, "Successfully downloaded <%d> files to: %s\n Exiting normally.",
          DL_SIZE(completedList), diffDestPrefix);
  logMessage(fLogPtr, logBuff);

EXIT_CLEAN:

  if(sourceURL){
	free(sourceURL);
	sourceURL = NULL;
  }

  if(tmpDir){
	free(tmpDir);
	tmpDir = NULL;
  }

  if(host){
    curl_free(host);
    host = NULL;
  }

  if(path){
    curl_free(path);
    path = NULL;
  }

  if(downloadHandle){
    easyCleanup(downloadHandle);
    downloadHandle = NULL;
  }

  if(curlParseHandle){
    urlCleanup(curlParseHandle);
    curlParseHandle = NULL;
  }

  closeCurlSession();

  if(newDiffersList)
    zapStringList((void **) &newDiffersList);

  if(completedList)
    zapStringList((void **) &completedList);

  if(fLogPtr){
    /* write "DONE" footer to log file **/
    logMessage(fLogPtr, "DONE");

    fclose(fLogPtr);

    fLogPtr = NULL;
    curlLogtoFP = NULL;
    cookieLogFP = NULL;
  }

  releaseLock(lockFD);

  removeFile(myFiles.lockFile);

  zapSetting(&mySetting);

  zapSkeleton(&myDir);

  zapGd_files(&myFiles);

  return value2Return;

} /* END main() **/

/* getSettings(): Gets program settings; from the command line and configuration
 * file; we do not look at configuration file when ALL settings are provided on
 * the command line.
 *
 * Parameters:
 *   settings: pointer to MY_SETTING structure, caller allocates memory.
 *   argc: main() command line argument count.
 *   argv: main() character pointer to command line arguments.
 *
 *   Function uses several globally defined macros; mostly for names.
 *
 * Return: ztSuccess on success, several error codes maybe returned.
 *
 * Known bug: wrong value in configuration file causes this function to fail;
 * even when corresponding command line is used to set that particular option.
 * Maybe add IGNORE flag to CONF_ENTRY? Then set flags after parseCmdLine()?
 *
 ******************************************************************************/

int getSettings(MY_SETTING *settings, int argc, char* const argv[]){

  ASSERTARGS(settings && argv);

  if(argc == 1){ /* no argument --> show short help is done in main() **/
    fprintf(stderr, "%s: getSettings(): Error invalid value for argc.\n", progName);
    return ztInvalidArg;
  }

  /* set some sensible defaults we may have to use.
   *  - {HOME} is current user home directory.
   *  - default work directory: {HOME}/getdiff/
   *  - default configuration file: {HOME}/getdiff.conf
   *
   ********************************************/

  char  *homeDir;
  char  *defConfFile;

  int   result;

  homeDir = getHome();
  if( ! homeDir ){
    fprintf(stderr, "%s: Error failed getHome() function.\n", progName);
    return ztUnknownError;
  }

  appendEntry2Path(&defConfFile, homeDir, CONF_NAME);

  /* parse command line arguments first; give user chance to set
   * configuration file.
   * Note that ALL settings are zeroed out going in **/
  memset(settings, 0, sizeof(MY_SETTING));

  result = parseCmdLine(settings, argc, argv);
  if (result != ztSuccess){
    fprintf(stderr, "%s: Error failed parseCmdLine().\n", progName);
    return result;
  }

  if((settings->configureFile) &&
     (isFileUsable(settings->configureFile) != ztSuccess)){

    fprintf(stderr, "%s: Error specified configuration file is not usable.\n"
            " File checked: %s\n", progName, settings->configureFile);
    return ztInvalidArg;
  }

  if(! settings->configureFile)

    settings->configureFile = STRDUP(defConfFile);

  free(defConfFile);

  result = isFileUsable(settings->configureFile);

  if(result != ztSuccess)

    fprintf(stdout, "%s: Warning not using any configuration file;\n"
            " file not specified by user and default configuration file is not usable.\n"
            " Checked for configure file <%s>\n"
            " File is not usable for: <%s>\n",
            progName, settings->configureFile, ztCode2Msg(result));

  int  haveConfArgs = (settings->usr && settings->pswd &&
                       settings->source && settings->rootWD &&
                       settings->startNumber && settings->endNumber &&
                       settings->newDifferOff && settings->verbose);

  /* skip processing configuration file when
   * ALL arguments are given on the command line **/

  if(result == ztSuccess && ! haveConfArgs ){

    /* our array for configure entries with value member set to 'NULL' **/
    CONF_ENTRY  confEntries[] = {
      {"USER", NULL, NAME_CT, 0},
      {"PASSWD", NULL, NAME_CT, 0},
      {"SOURCE", NULL, INET_URL_CT, 0},
      {"DIRECTORY", NULL, DIR_CT, 0},
      {"BEGIN", NULL, DIGITS9_CT, 0},
      {"END", NULL, DIGITS9_CT, 0},
      {"VERBOSE", NULL, BOOL_CT, 0},
      {"NEWER_FILE", NULL, NONE_CT, 0}, /* NONE_CT accepts 'none' and 'off' for value **/
      {NULL, NULL, 0, 0}
    };

    result = initialConf(confEntries, 8);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed initialConf() function for <%s>. Exiting.\n",
              progName, ztCode2Msg(result));
      return result;
    }

    int numFound;

    confErrLineNum = 0; /* on error set by configureGetValue() **/

    result = configureGetValues(confEntries, &numFound, settings->configureFile);

    /*
     * configureGetValues() reads and checks all
     * entries regardless of command line settings!
     * we could remove entries set on the command line???
     * wrong entries in the configuration file will NOT
     * get checked then.
     * we may have an array of set command line values and
     * then not check configure corresponding value?!
     * or add IGNORE member in CONF_ENTRY?! which we set
     * after reading command line.
     *
     ****************************************************/

    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed configureGetValues() function for <%s>. Exiting.\n",
              progName, ztCode2Msg(result));

      if(confErrLineNum)

        fprintf(stderr, "Error in configuration file at line number: <%d>\n",  confErrLineNum);

      return result;
    }

    /* mergeConfigure(): brings in values from configuration file to settings **/
    result = mergeConfigure(settings, confEntries);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed mergeConfigure() function.\n", progName);
      return result;
    }

    // TODO: zap conf entries() :: reverse initialConf(confEntries, 8);

  } /* end if(... process configuration file ...) **/

  /* if user did not specify where to have work directory; use $HOME **/
  if( ! settings->rootWD)

    settings->rootWD = STRDUP(homeDir);

  result = isDirUsable(settings->rootWD);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isDirUsable() function for work directory"
            " root! Directory tested: <%s>\n"
            "Directory must exist and user ownership for this directory is required.\n"
            "function failed for: <%s>\n",
            progName, settings->rootWD, ztCode2Msg(result));
    return result;
  }

  /* root for work directory can not end with "getdiff" entry **/
  char   *lastEntry;

  lastEntry = lastOfPath(settings->rootWD);
  if(strcmp(lastEntry,WORK_ENTRY) == 0){

    fprintf(stderr, "%s: Error root for work directory can not end with <%s> entry.\n",
            progName, WORK_ENTRY);
    return ztInvalidArg;
  }

  return ztSuccess;

} /* END getSettings() **/

/* mergeConfigure(): merges configure 'value' into settings.
 * Member 'value' is used only if NOT set in settings
 *
 *
 *****************************************************************/

int mergeConfigure(MY_SETTING *settings, CONF_ENTRY confEntries[]){

  CONF_ENTRY   *mover;

  ASSERTARGS (settings && confEntries);

  /* use configure setting only when not in 'settings' structure **/
  mover= confEntries;
  while (mover->key){

    switch (mover->index){ /* index member is set in initialConf() **/

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

    case 4: // BEGIN

      if ( !settings->startNumber && mover->value){

        settings->startNumber = STRDUP (mover->value);
      }
      break;

    case 5: // END

      if ( !settings->endNumber && mover->value){

        settings->endNumber = STRDUP (mover->value);
      }
      break;

    case 6: // VERBOSE

      if (settings->verbose == 1)

        break;

      if(mover->value){

        char *lowerValue;

        string2Lower(&lowerValue, mover->value); // do NOT use strcasecmp()
        if(!lowerValue){
          fprintf(stderr, "mergeConfigure(): Error failed string2Lower() for value!\n");
          return ztMemoryAllocate;
        }

        if ((strcmp(lowerValue, "true") == 0) ||
            (strcmp(lowerValue, "on") == 0) ||
            (strcmp(mover->value, "1") == 0))

          settings->verbose = 1;

      }

      break;

    case 7: // newDifferOff

      if(!settings->newDifferOff && mover->value)

        settings->newDifferOff = 1;

      break;

    default:

      break;
    } // end switch

    mover++;

  } // end while()

  return ztSuccess;

} /* END mergeConfigure() **/

int chkRequired(MY_SETTING *settings, char *previousFile){

  ASSERTARGS(settings && previousFile);

  int result;

  /* source must be supported; curl URL parser is used. **/
  if( isSourceSupported(sourceURL, curlParseHandle) != ztSuccess){
//  if( isSourceSupported_old(sourceURL) != ztSuccess){
    fprintf (stderr, "%s: Error specified source URL is not supported by this program.\n", progName);
    return ztInvalidArg;
  }

  /* can we reach 'source'? network connection is required.
   * note: curl session is not required for isConnCurl() function. **/
  result = isConnCurl(settings->source);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error could not connect to 'source': <%s>\n"
            "This program requires internet connection.", progName, settings->source);
    return result;
  }

  /* user name and password argument are required when
   * geofabrik internal server is in use **/

  int useInternal = 0;

  if(strstr(sourceURL, INTERNAL_SERVER))

    useInternal = 1;

  if(useInternal && ( ! settings->usr)){
    fprintf(stderr, "%s: Error missing 'usr' argument;\n"
            "User name for 'OSM ACCOUNT' is required for geofabrik.de internal server.\n"
            "Note that email associated with the account can be used for user name.\n", progName);

    return ztMissingArg;
  }

  if(useInternal && (! settings->pswd)){
    fprintf(stderr, "%s: Error missing 'passwd' argument.\n"
            "Password for 'OSM ACCOUNT' is required for geofabrik.de internal server.\n", progName);

    return ztMissingArg;
  }

  /* 'begin' argument is required for program first time use only.
   * On exit program writes "previous.state.txt" file in its working
   * directory, it is program first run when this file is NOT found.
   * Note that we do not look inside the file!
   *
   ************************************************************/

  int   firstUse;

  result = isFileUsable(previousFile);

  firstUse = (result != ztSuccess);

  if(firstUse && (! settings->startNumber)){
    fprintf(stderr, "%s: Error missing 'begin' argument; argument is required for program first use.\n", progName);

    return ztMissingArg;
  }

  if(settings->endNumber && (settings->startNumber == NULL)){
    fprintf(stderr, "%s: Error missing 'begin' argument with 'end' set; argument is required to download a range of files.\n", progName);

    return ztMissingArg;
  }

  return ztSuccess;

} /* END chkRequired() **/

char *setDiffersDirPrefix(SKELETON *skl, const char *src){

  char *dest = NULL;

  ASSERTARGS(skl && src);

  if(strstr(src, "geofabrik"))
    dest = skl->geofabrik;
  else if(strstr(src, "minute"))
    dest = skl->planetMin;
  else if(strstr(src, "hour"))
    dest = skl->planetHour;
  else if(strstr(src, "day"))
    dest = skl->planetDay;
  else
    ;

  return dest;

} /* END setDiffersDirPrefix() **/

char *getLoginToken(MY_SETTING *setting, SKELETON *myDir){

  ASSERTARGS(setting && myDir);

  char *token = NULL;
  int  result;

  result = doCookie(setting, myDir);
  if(result != ztSuccess){

    /* handle ztExpiredCookie return as fatal error **/

    fprintf(stderr, "%s: Error failed doCookie() function.\n", progName);
    logMessage(fLogPtr, "Error failed doCookie() function.");

    return token;
  }

  token = getCookieToken();

  if(!token){

    fprintf(stderr, "%s: Error failed getCookieToken() function.\n", progName);
    logMessage(fLogPtr, "Error failed getCookieToken() function.");

    return token;
  }

  destroyCookie(); /* tell cookie logic to cleanup **/

  return token;

} /* END getLoginTiken() **/

/* isStateFileList():
 *  - must be a STRING_LT
 *  - must have exactly THREE lines
 *  - one line must start with "timestamp=" substring
 *  - one line must start with "sequenceNumber=" substring
 *
 * **************************************************/
int isStateFileList(STRING_LIST *list){

  ASSERTARGS(list);

  if( ! TYPE_STRING_LIST(list) )

    return FALSE;

  if(DL_SIZE(list) != 3)

    return FALSE;

  ELEM   *elem;
  char   *timeLine = NULL;
  char   *sequenceLine = NULL;

  elem = findElemSubString(list, "timestamp=");
  if(elem)
    timeLine = (char*) DL_DATA(elem);

  elem = findElemSubString(list, "sequenceNumber=");
  if(elem)
    sequenceLine = (char*) DL_DATA(elem);

  if(! timeLine)
    fprintf(stderr, "isStateFileList(): Error could not find timeLine.\n");

  if(! sequenceLine)
    fprintf(stderr, "isStateFileList(): Error could not find sequenceLine.\n");

  if(timeLine && sequenceLine)

    return TRUE;

  return FALSE;

} /* END isStateFileList() **/

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
 * 'remotePathSuffix' is the path part from the sequence number, might include
 * change file name and state.txt file name.
 *
 * originalPath is restored before exit (return).
 *
 ******************************************************************************/

int myDownload(char *remotePathSuffix, char *localFile){

  int   result;

  CURLUcode   curluResult; /* returned type by curl_url_get() & curl_url_set() **/
  char        *originalPath;
  char        newPath[PATH_MAX] = {0};

  char        *currentSourceURL;

  ASSERTARGS(remotePathSuffix && localFile);

  result = isGoodFilename(localFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isGoodFilename() for 'localFile' parameter in myDownload(): <%s>.\n",
            progName, localFile);
    return result;
  }

  /* ensure that "curlParseHandle" is intact
   * currentSourceURL is NEVER NULL; it can be empty string **/
  currentSourceURL = getUrlStringCURLU(curlParseHandle);

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

    curluResult = curl_url_get(curlParseHandle, CURLUPART_PATH, &originalPath, 0);
    if(curluResult != CURLUE_OK ) {
      fprintf(stderr, "%s: Error failed curl_url_get() for path part.\n"
              "Curl error message: <%s>\n", progName,
              curl_url_strerror(curluResult));
      return ztFailedLibCall;
    }

    /* use ONLY ONE slash between parts **/

    if(remotePathSuffix[0] == '\057')

      remotePathSuffix++;

    if(SLASH_ENDING(originalPath))
      sprintf(newPath, "%s%s", originalPath, remotePathSuffix);
    else
      sprintf(newPath, "%s/%s", originalPath, remotePathSuffix);


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

    if(result == ztNetConnFailed){
      fprintf(stderr, "download failed for lost established connection; check cables please.\n");
      logMessage(fLogPtr, "download failed for lost established connection; check cables please.");
    }

    /* restore originalPath **/
    curluResult = curl_url_set (curlParseHandle, CURLUPART_PATH, originalPath, 0);
    if(curluResult != CURLUE_OK ) {
      fprintf(stderr, "%s: Error failed curl_url_set() for path part.\n"
              "Curl error message: <%s>\n", progName, curl_url_strerror(curluResult));

      //return ztFailedLibCall;
    }

    return result;
  }

  /* restore originalPath before return **/
  curluResult = curl_url_set (curlParseHandle, CURLUPART_PATH, originalPath, 0);
  if(curluResult != CURLUE_OK ) {
    fprintf(stderr, "%s: Error failed curl_url_set() for path part.\n"
            "Curl error message: <%s>\n", progName, curl_url_strerror(curluResult));

    return ztFailedLibCall;
  }

  return ztSuccess;

} /* END myDownload() **/

char *fetchLatestSequence(char *remoteName, char *localDest){

  ASSERTARGS(remoteName && localDest);

  int result;
  char *latestSequence = NULL;

  if(fVerbose){
    fprintf(stdout, "fetchLatestSequence(): Downloading latest \"state.txt\" file from remote server.\n");
    logMessage(fLogPtr, "fetchLatestSequence(): Downloading latest \"state.txt\" file from remote server.");
  }

  result = myDownload(remoteName, localDest);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myDownload() in fetchLatestSequence().\n", progName);
    logMessage(fLogPtr, "Error failed myDownload() in fetchLatestSequence().");

    return latestSequence;
  }

  if(fVerbose){
    fprintf(stdout, "fetchLatestSequence(): Wrote latest \"state.txt\" to local file: <%s>.\n", localDest);
    logMessage(fLogPtr, "fetchLatestSequence(): Wrote latest \"state.txt\" to local file below:");
    logMessage(fLogPtr, localDest);
  }

  latestSequence = stateFile2SequenceString(localDest);

  if(fVerbose){
    fprintf(stdout, "fetchLatestSequence(): Extracted latest SEQUENCE NUMBER from remote server: <%s>\n", latestSequence);
    logMessage(fLogPtr, "fetchLatestSequence(): Extracted latest SEQUENCE NUMBER from remote server below:");
    logMessage(fLogPtr, latestSequence);
  }

  return latestSequence;

} /* fetchLatestSequence() **/

/* areNumsGoodPair(): are numbers good pair?
 *
 *  FIXME fix comments and feed back msgs FIXME
 *  - numbers are already good sequence numbers; checked at getSettings().
 *  - we avoid changing strings to integers.
 *  - we assume that minute, hour and day granularities all have different sequence
 *    numbers grouped as follows:
 *    * minute sequence numbers are 7 digits such as: 6321780
 *    * hour sequence numbers are 6 digits such as: 106276
 *    * day sequence numbers are 4 digits such as: 4428
 *
 * 1- end & start must share root entry; must be from the same granularity.
 *    must share root entry also because we do NOT fetch thousands of files.
 * 2- if they do not share 'parentEntry'; then parents must be adjacent entries.
 * 3- files (change file + state.txt file) for both start and end sequence
 *    numbers must exist on remote server; we only check for state.txt file
 *    for both and assume corresponding change file has the same status.
 * 4- end time stamp must be newer than start time stamp.
 *
 *****************************************/

int areNumsGoodPair(const char *startNum, const char *endNum){

  ASSERTARGS(startNum && endNum);

  if(strlen(endNum) != strlen(startNum)){
    if(strlen(endNum) > (strlen(startNum) + 1)){

      fprintf(stderr, "%s: Error end sequence number is much larger than start sequence number - has more digits.\n"
              "Sequence numbers must be of the same GRANULARITY (minutely, hourly or daily) and no more than %d change files apart.\n",
              progName, MAX_OSC_DOWNLOAD);
      logMessage(fLogPtr, "Error end sequence number is much larger than start sequence number - has more digits.\n"
                 "Sequence numbers must be of the same GRANULARITY (minutely, hourly or daily) and no more than 61 change files apart.");

      return ztInvalidArg;
    }
    else if(strlen(endNum) < strlen(startNum)){
      fprintf(stderr, "%s: Error end sequence number is smaller than start sequence number!\n"
              "End number must be the newer (larger) sequence number.\n"
              "Sequence numbers must be of the same GRANULARITY (minutely, hourly or daily) and no more than %d change files apart.\n",
              progName, MAX_OSC_DOWNLOAD);
      logMessage(fLogPtr, "Error end sequence number is smaller than start sequence number!\n"
                 "End number must be the newer (larger) sequence number.\n"
                 "Sequence numbers must be of the same GRANULARITY (minutely, hourly or daily) and no more than 61 change files apart.");

      return ztInvalidArg;
    }
  }

  int result;
  PATH_PART startPP, endPP;

  result = sequence2PathPart(&startPP, startNum);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed sequence2PathPart() function.\n", progName);
    logMessage(fLogPtr, "Error failed sequence2PathPart() function.");

    return result;
  }

  result = sequence2PathPart(&endPP, endNum);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed sequence2PathPart() function.\n", progName);
    logMessage(fLogPtr, "Error failed sequence2PathPart() function.");

    return result;
  }

  if(fVerbose){
    fprintf(stdout, "start PATH_PART is below:\n");
    fprintPathPart(NULL, &startPP);

    printf("end PATH_PART is below:\n");
    fprintPathPart(NULL, &endPP);
  }

  if(strcmp(startPP.rootEntry, endPP.rootEntry) != 0){

    fprintf(stderr, "%s: Error; start and end sequence numbers do not share root entry.\n", progName);
    logMessage(fLogPtr, "Error; sequence numbers (both) must of the same granularity.");

    return ztInvalidArg;
  }

  if(strcmp(startPP.parentEntry, endPP.parentEntry) != 0){

    if(! areAdjacentStrings(startPP.rootEntry, startPP.parentEntry, endPP.parentEntry) ){
      fprintf(stderr, "%s: Error; start & end sequence numbers are NOT from adjacent directories!\n"
              " start parent directory is: %s\n"
              " end parent directory is: %s\n"
              " Sequence numbers must be from same GRANULARITY with begin being the older of\n"
              " the two and no more than %d change files can be downloaded each invocation.\n\n",
              progName, startPP.parentPath, endPP.parentPath, MAX_OSC_DOWNLOAD);
      logMessage(fLogPtr, "Error; start & end sequence numbers are NOT from adjacent directories!");

      return ztInvalidArg;
    }
  } /* end if not same parent **/

  /* check if files exist on remote server **/
  char fileSuffix[PATH_MAX] = {0};

  sprintf(fileSuffix, "%s%s", startPP.filePath, STATE_EXT);

  printf("calling isRemote() with fileSuffix = %s\n", fileSuffix);

  result = isRemoteFile(fileSuffix);
  if(result == ztFileNotFound){
    fprintf(stderr, "%s: Error, change file for start sequence number is not available.\n"
            "Please check server site for available files.\n\n", progName);
    logMessage(fLogPtr, "Error, change file for start sequence number is not available.\n"
               "Please check server site for available files.\n");

    return ztFileNotFound;
  }
  else if(result != ztSuccess){
    fprintf(stderr, "%s: Error, failed isRemoteFile() function for start sequence number.\n"
            " You may retry later but function failed for: <%s>.\n\n",
            progName, ztCode2ErrorStr(result));
    logMessage(fLogPtr, "Error, failed isRemoteFile() function for start sequence number.\n"
               " You may retry later but function failed for: -- see below --");
    logMessage(fLogPtr, ztCode2ErrorStr(result));

    return result;;
  }
  else{ /* result == ztSuccess **/
    if(fVerbose){
      fprintf(stdout, "%s: Change file for start sequence number is available from remote.\n", progName);
      logMessage(fLogPtr, "Change file for start sequence number is available from remote.");
    }
  }

  /* is file for end sequence available? **/
  sprintf(fileSuffix, "%s%s", endPP.filePath, STATE_EXT);

  result = isRemoteFile(fileSuffix);
  if(result == ztFileNotFound){
    fprintf(stderr, "%s: Error, change file for end sequence number is not available.\n"
            "Please check server site for available files.\n\n", progName);
    logMessage(fLogPtr, "Error, change file for end sequence number is not available.\n"
               "Please check server site for available files.\n");

    return ztFileNotFound;
  }
  else if(result != ztSuccess){
    fprintf(stderr, "%s: Error, failed isRemoteFile() function for end sequence number.\n"
            " You may retry later but function failed for: <%s>.\n\n",
            progName, ztCode2ErrorStr(result));
    logMessage(fLogPtr, "Error, failed isRemoteFile() function for end sequence number.\n"
               " You may retry later but function failed for: -- see below --");
    logMessage(fLogPtr, ztCode2ErrorStr(result));

    return result;
  }
  else{ /* result == ztSuccess **/
    if(fVerbose){
      fprintf(stdout, "%s: Change file for end sequence number is available from remote.\n", progName);
      logMessage(fLogPtr, "Change file for end sequence number is available from remote.");
    }
  }

  /* 'end' must be newer than 'start' - compare time stamps from state.txt files **/

  result = isEndNewer(&startPP, &endPP);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error 'end' does not have more recent time stamp than 'start'.\n", progName);
    logMessage(fLogPtr, "Error 'end' does not have more recent time stamp than 'start'.");

    return result;
  }

  return ztSuccess;

} /* END areNumsGoodPair() **/

/* isRemoteFile2(): is file available - exist - on remote server?
 *
 * - function tries to retrieve 'header' ONLY for file from remote server
 * - if 'header' was NOT found, then file does NOT exist on remote
 * - 'header' is saved to tmpDir with appended ".Header" to filename.
 *
 * Caller should check returns for any error.
 *
 ******************************************************************/

int isRemoteFile(char *remoteSuffix){

  char *toDir = (char *) tmpDir; // set to global tmpDir

  ASSERTARGS(remoteSuffix && toDir);

  int  result;

  /* this function uses myDownload() to get header for file from remote:
     prototype: int myDownload(char *remotePathSuffix, char *localFile)
     first argument is our argument here 'remoteSuffix' we make string
     for the second argument, saving header to 'tmpDir'.

     Another approach is to partially retrieve first chunk bytes (say 1024) to
     memory; not to file, then compare to (look for) some hint string.
  *********************************************************************/

  char localFile[PATH_MAX] = {0};

  if(SLASH_ENDING(toDir))
    sprintf(localFile, "%s%s.Header", toDir, lastOfPath(remoteSuffix));
  else
    sprintf(localFile, "%s/%s.Header", toDir, lastOfPath(remoteSuffix));

  /* set 2 options in global 'downloadHandle': CURLOPT_NOBODY & CURLOPT_HEADER **/

  CURLcode curlCode;

  curlCode = curl_easy_setopt(downloadHandle, CURLOPT_NOBODY, 1L);
  if (curlCode != CURLE_OK){
    fprintf(stderr, "%s: Error failed curl_easy_setopt() for CURLOP_NOBODY.\n", progName);
    logMessage(fLogPtr, "Error failed curl_easy_setopt() for CURLOP_NOBODY.");

    return ztFailedLibCall;
  }

  curlCode = curl_easy_setopt(downloadHandle, CURLOPT_HEADER, 1L);
  if (curlCode != CURLE_OK){
    fprintf(stderr, "%s: Error failed curl_easy_setopt() for CURLOP_HEADER.\n", progName);
    logMessage(fLogPtr, "Error failed curl_easy_setopt() for CURLOP_HEADER.");

    return ztFailedLibCall;
  }

  result = myDownload(remoteSuffix, localFile);

  /* restore 2 options to defaults **/
  curlCode = curl_easy_setopt(downloadHandle, CURLOPT_NOBODY, 0L);
  if (curlCode != CURLE_OK){
    fprintf(stderr, "%s: Error failed curl_easy_setopt() for CURLOP_NOBODY (unset).\n", progName);
    logMessage(fLogPtr, "Error failed curl_easy_setopt() for CURLOP_NOBODY (unset).");

    return ztFailedLibCall;
  }

  result = curl_easy_setopt(downloadHandle, CURLOPT_HEADER, 0L);
  if (curlCode != CURLE_OK){
    fprintf(stderr, "getRemoteHeader(): Error failed curl_easy_setopt() for CURLOP_HEADER (restore).\n");
    logMessage(fLogPtr, "getRemoteHeader(): Error failed curl_easy_setopt() for CURLOP_HEADER (restore).");

    return ztFailedLibCall;
  }

#ifdef REMOVE_TMP
  removeFile(localFile);
#endif

  if(result == ztResponse404)

    return ztFileNotFound;

  else if(result == ztResponse200)

    return ztSuccess;

  return result;

} /* END isRemoteFile2() **/

/* areAdjacentStrings(): are firstStr & secondStr adjacent in string list?
 * string list is constructed from sourceSuffix.
 *
 * secondStr comes AFTER firstStr only.
 *
 ************************************************************************/

int areAdjacentStrings(char *sourceSuffix, char *firstStr, char *secondStr){

  char *toDir = (char *) tmpDir; // use tmpDir

  ASSERTARGS(sourceSuffix && firstStr && secondStr && tmpDir);

  int result;
  char *filename = "areAdjacent.html";
  char myFile[512] = {0};

  STRING_LIST *list;

  list = initialStringList();

  if(!list){
    fprintf(stderr, "%s: Error failed initialStringList() in areAdjacentStrings(). Terminating program.\n", progName);
    logMessage(fLogPtr, "Error failed initialStringList()in areAdjacentStrings(). Terminating program.");

    exit(ztMemoryAllocate);
  }

  if(SLASH_ENDING(toDir))
    sprintf(myFile, "%s%s", toDir, filename);
  else
    sprintf(myFile, "%s/%s", toDir, filename);

  /* curl parse handle has the starting part of the 'path' or the 'path' prefix,
   * we provide the rest of the path - or the 'path' suffix - to myDownload()
   * as the first parameter. Second parameter is our local file name.
   ***********************************************************************/

  result = myDownload(sourceSuffix, myFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myDownload(), result is: %s\n\n", progName, ztCode2ErrorStr(result));
    logMessage(fLogPtr, "Error failed myDownload() function; see reason below:");
    logMessage(fLogPtr, ztCode2ErrorStr(result));

    zapStringList((void **) &list);
    return FALSE;
  }

  result = parseHtmlFile(list, myFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed parseHtmlFile().\n", progName);
    logMessage(fLogPtr, "Error failed parseHtmlFile().");

    zapStringList((void **) &list);
    return FALSE;
  }
  /*
    printf("areAjacentStrings(): after parseHtmlFile() printing 'list' below:\n");
    fprintStringList(stdout, list);
  */
  ELEM *firstElem, *secondElem;

  firstElem = findElemString(list, firstStr);
  if(!firstElem){
    fprintf(stderr, "%s: Error failed findElemString() for 'firstStr': <%s>.\n", progName, firstStr);
    logMessage(fLogPtr, "Error failed findElemString() for 'firstStr'.");

    zapStringList((void **) &list);
    return FALSE;
  }

  secondElem = findElemString(list, secondStr);
  if(!secondElem){
    fprintf(stderr, "%s: Error failed findElemString() for 'secondStr': <%s>.\n", progName, secondStr);
    logMessage(fLogPtr, "Error failed findElemString() for 'secondStr'.");

    zapStringList((void **) &list);
    return FALSE;
  }

  /* result is TRUE only if second element is next to first element **/
  if(secondElem == DL_NEXT(firstElem)){

#ifdef REMOVE_TMP
    removeFile(myFile);
#endif

    zapStringList((void **) &list);
    return TRUE;
  }

  return FALSE;

} /* END areAdjacentStrings()  **/

/* isEndNewer(): returns ztSuccess when 'end' has a newer
 *               or more recent time stamp than that of 'start'.
 *
 ***************************************************************/

int isEndNewer(PATH_PART *startPP, PATH_PART *endPP){

  char *toDir = (char *) tmpDir; // must be set upstream

  ASSERTARGS(startPP && endPP && toDir);

  int result;

  /* get state.txt files for both, to compare their age. **/
  char remoteSuffix[128] = {0};
  char startLocalFile[PATH_MAX] = {0};
  char endLocalFile[PATH_MAX] = {0};

  STATE_INFO *startStateInfo, *endStateInfo;

  /* initial state infos **/
  startStateInfo = initialStateInfo();
  if(!startStateInfo){
    fprintf(stderr, "%s: Error failed initialStateInfo() call for start number.\n", progName);
    logMessage(fLogPtr, "Error failed initialStateInfo() call for start number.");

    return ztMemoryAllocate;
  }

  endStateInfo = initialStateInfo();
  if(!endStateInfo){
    fprintf(stderr, "%s: Error failed initialStateInfo() call for end number.\n", progName);
    logMessage(fLogPtr, "Error failed initialStateInfo() call for end number.");

    return ztMemoryAllocate;
  }

  sprintf(remoteSuffix, "%s%s", startPP->filePath, STATE_EXT);

  if(SLASH_ENDING(toDir))
    sprintf(startLocalFile, "%s%s%s", toDir, startPP->fileEntry, STATE_EXT);
  else
    sprintf(startLocalFile, "%s/%s%s", toDir, startPP->fileEntry, STATE_EXT);

  /* download start state.txt file **/
  result = myDownload(remoteSuffix, startLocalFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myDownload() function for start state.txt file.\n", progName);
    logMessage(fLogPtr, "Error failed myDownload() function for start state.txt file.");

    return result;
  }

  sprintf(remoteSuffix, "%s%s", endPP->filePath, STATE_EXT);

  if(SLASH_ENDING(toDir))
    sprintf(endLocalFile, "%s%s%s", toDir, endPP->fileEntry, STATE_EXT);
  else
    sprintf(endLocalFile, "%s/%s%s", toDir, endPP->fileEntry, STATE_EXT);

  /* download end state.txt file **/
  result = myDownload(remoteSuffix, endLocalFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myDownload() function for end state.txt file.\n", progName);
    logMessage(fLogPtr, "Error failed myDownload() function for end state.txt file.");

    return result;
  }

  /* parse state.txt files (start & end) into STATE_INFOs **/
  result = stateFile2StateInfo(startStateInfo, startLocalFile);

#ifdef REMOVE_TMP
  removeFile(startLocalFile);
  if(result != ztSuccess)
	removeFile(endLocalFile);
#endif

  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed stateFile2StateInfo() function for start state.txt file.\n", progName);
    logMessage(fLogPtr, "Error failed stateFile2StateInfo() function for start state.txt file.");

    return result;
  }

  result = stateFile2StateInfo(endStateInfo, endLocalFile);

#ifdef REMOVE_TMP
  removeFile(endLocalFile);
#endif

  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed stateFile2StateInfo() function for end state.txt file.\n", progName);
    logMessage(fLogPtr, "Error failed stateFile2StateInfo() function for end state.txt file.");

    return result;
  }

  /*
    fprintf(stdout, "printing STATE_INFO for START below:\n");
    fprintStateInfo2(stdout, startStateInfo);

    fprintf(stdout, "printing STATE_INFO for END below:\n");
    fprintStateInfo2(stdout, endStateInfo);
  **/

  if(endStateInfo->timeValue < startStateInfo->timeValue){
    fprintf(stderr, "%s: Error end change file is not newer than start change file.\n", progName);
    logMessage(fLogPtr,"Error end change file is not newer than start change file.");

    return ztInvalidArg;
  }

  return ztSuccess;

} /* END isEndNewer() **/

int getDiffersList(STRING_LIST *destList, PATH_PART *startPP, PATH_PART *endPP){

  ASSERTARGS(destList && startPP && endPP);

  ASSERTARGS(startPP->parentPath && startPP->fileEntry); // needed: is filled out structure?
  ASSERTARGS(endPP->parentPath && endPP->fileEntry);

  if(DL_SIZE(destList)){
    fprintf(stderr, "%s: Error destList is not empty.\n", progName);
    logMessage(fLogPtr, "Error destList is not empty.");
    return ztListNotEmpty;
  }

  int result;
  STRING_LIST *parentsList;
  STRING_LIST *childrenList;

  char *newPath;

  /* fill parentList: list of Parent Page(s) from startSequenceNum & endSequenceNum
   * there will be one page when start & end share parent or two when they do not
   *******************************************************************************/
  parentsList = initialStringList();
  if(!parentsList){
    fprintf(stderr, "%s: Error failed initialStringList().\n", progName);
    logMessage(fLogPtr, "Error failed initialStringList().");

    return ztMemoryAllocate;
  }

  newPath = STRDUP(startPP->parentPath);

  /* insert start 'parentPath' as FIRST element.
   * ALWAYS use dynamically allocated memory for data pointer in ELEM. **/

  result = insertNextDL(parentsList, DL_TAIL(parentsList), (void *) newPath);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed insertNextTo() function.\n", progName);
    logMessage(fLogPtr, "Error failed insertNextTo() function.");

    return result;
  }

  /* if they do NOT share parent entry, add end parentPath to list as SECOND element. **/
  if(strcmp(startPP->parentPath, endPP->parentPath) != 0){

    newPath = STRDUP(endPP->parentPath);

    result = insertNextDL(parentsList, DL_TAIL(parentsList), (void *) newPath);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed insertNextTo() function.\n", progName);
      logMessage(fLogPtr, "Error failed insertNextTo() function.");

      return result;
    }
  }

  ELEM *parentElem;
  char *currentParent;

  ELEM *startElem;
  ELEM *endElem;
  ELEM *diffElem;

  char *filename;
  char buffer[64] = {0};
  char *newDiff;

  parentElem = DL_HEAD(parentsList);

  while(parentElem){

    currentParent = (char *) DL_DATA(parentElem);

    childrenList = initialStringList();
    if(!childrenList){
      fprintf(stderr, "%s: Error failed initialStringList().\n", progName);
      logMessage(fLogPtr, "Error failed initialStringList().");

      return ztMemoryAllocate;
    }

    result = getParentPage(childrenList, currentParent);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed getParentPage() function.\n", progName);
      logMessage(fLogPtr, "Error failed getParentPage() function.");

      return result;
    }

    char logStr[1024] = {0};

    fprintf(stdout, "%s: Retrieved childrenList from remote server with size: %d\n", progName, DL_SIZE(childrenList));
    sprintf(logStr, "Retrieved childrenList from remote server with size: %d", DL_SIZE(childrenList));
    logMessage(fLogPtr, logStr);
    /*
      if(fVerbose){
      fprintStringList(NULL, childrenList);
      fprintStringList(fLogPtr, childrenList);
      }
    **/
    if(parentElem == DL_HEAD(parentsList)){
      startElem = findElemSubString(childrenList, startPP->fileEntry);

      if(fUsingPreviousID){ /* start AFTER last one we downloaded **/
        startElem = DL_NEXT(DL_NEXT(startElem));
      }
    }
    else
      startElem = DL_HEAD(childrenList);

    if(parentElem == DL_TAIL(parentsList))
      endElem = DL_NEXT(findElemSubString(childrenList, endPP->fileEntry)); // files are in pairs (.osc & .state.txt)
    else
      endElem = DL_TAIL(childrenList);

    diffElem = startElem;
    while(diffElem){

      filename = (char *) DL_DATA(diffElem);
      sprintf(buffer, "%s%s", currentParent, filename);

      newDiff = STRDUP(buffer);

      result = insertNextDL(destList, DL_TAIL(destList), (void *) newDiff);
      if(result != ztSuccess){
        fprintf(stderr, "%s: Error failed insertNextDL().\n", progName);
        logMessage(fLogPtr, "Error failed insertNextDL().");
        return result;
      }

      if(diffElem == endElem)
        break;

      diffElem = DL_NEXT(diffElem);
    }

    parentElem = DL_NEXT(parentElem);

  }/* end while(parentElem) **/

  //TODO: cleanup

  return ztSuccess;

} /* END getDiffersList() **/


int downloadFilesList(STRING_LIST *completed, STRING_LIST *downloadList, char *localDestPrefix, int textOnly){

  int    result;
  ELEM   *elem;
  char   *filename;
  char   *pathSuffix;
  char   localFilename[1024];

  int    iCount = 0;
  int    sleepSeconds = 0;

  ASSERTARGS(completed && downloadList && localDestPrefix);

  /* completed: filename is inserted once download is completed successfully
   * the list should be empty. caller initials list.
   */

  elem = DL_HEAD(downloadList);
  while(elem){

    pathSuffix = (char *)DL_DATA(elem);
    filename = lastOfPath(pathSuffix);

    if(textOnly && ! strstr(filename, ".state.txt")){
      elem = DL_NEXT(elem);
      continue;
    }

    iCount++;

    /* no delay for first 12 files **/
    if(iCount < 11)
      sleepSeconds = SLEEP_INTERVAL;

    /* increment sleep time after each 4 files **/
    else if( (iCount % 4) == 0 )
// disable increasing delay     sleepSeconds += SLEEP_INTERVAL;
      sleepSeconds = SLEEP_INTERVAL;

    else ;

    memset(localFilename, 0, sizeof(localFilename));
    if(SLASH_ENDING(localDestPrefix))
      sprintf(localFilename, "%s%s", localDestPrefix, pathSuffix + 1); // (pathSuffix + 1) skip FIRST slash in path
    else
      sprintf(localFilename, "%s/%s", localDestPrefix, pathSuffix + 1);

    /* wait before next download **/
    sleep(sleepSeconds);

    result = myDownload(pathSuffix, localFilename);
    if(result == ztSuccess){
      /* each list must have its own copy of data; this is
       * so zapString() does not free same pointer again. **/

      char *pathSuffixCopy;
      pathSuffixCopy = STRDUP(pathSuffix);

      insertNextDL(completed, DL_TAIL(completed), (void *) pathSuffixCopy); //(void **) pathSuffixCopy);
    }
    else{
      fprintf(stderr, "%s: Error failed myDownload() function for localFilename: <%s>\n",
              progName, localFilename);
      char logBuff[2048] = {0};
      sprintf(logBuff, "Error failed myDownload() function for localFilename: <%s>\n", localFilename);
      logMessage(fLogPtr, logBuff);
      return result;
    }

    /* file sizes are checked in download2File() function **/

    elem = DL_NEXT(elem);
  }

  return ztSuccess;

} /* END downloadFilesList() **/

int getParentPage(STRING_LIST *destList, char *parentSuffix){

  ASSERTARGS(destList && parentSuffix);

  if(DL_SIZE(destList)){
    fprintf(stderr, "%s: Error 'destList' parameter must be an initialed EMPTY STRING list.\n", progName);
    logMessage(fLogPtr, "ror 'destList' parameter must be an initialed EMPTY STRING list.");

    return ztListNotEmpty;
  }

  char localParentFile[PATH_MAX] = {0};
  int  result;

  if(SLASH_ENDING(tmpDir))
    sprintf(localParentFile, "%s%s%s", tmpDir, lastOfPath(parentSuffix), HTML_EXT);
  else
    sprintf(localParentFile, "%s/%s%s", tmpDir, lastOfPath(parentSuffix), HTML_EXT);

  fprintf(stdout, "%s: Downloading parent page to local file: %s\n", progName, localParentFile);

  result = myDownload(parentSuffix, localParentFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myDownload() function for parent page: <%s>.\n", progName, parentSuffix);
    logMessage(fLogPtr, "Error failed myDownload() function for parentPage file.");

    return result;
  }

  result = parseHtmlFile(destList, localParentFile);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed parseHtmlFile() for file: %s", progName, localParentFile);
    logMessage(fLogPtr, "Error failed failed parseHtmlFile() function.");

    return result;
  }

  if(! IS_EVEN(DL_SIZE(destList))){
    fprintf(stderr, "%s: Error childrenList size is not even number! Size is: <%d>\n"
            "Files should be in pairs; change file and its corresponding state.txt file from server.\n",
            progName, DL_SIZE(destList));
    logMessage(fLogPtr, "Error childrenList size is not even number!\n"
               "Files should be in pairs; change file and its corresponding state.txt file from server.");

    return ztMalformedFile; /* it was not a parse error, still this code may not be right! **/
  }

#ifdef REMOVE_TMP
  removeFile(localParentFile);
#endif

  return ztSuccess;

} /* END getParentPage() **/

/* Granularity string is one of: [minute, hour, day] **/

int prependGranularity(STRING_LIST **list, char *gString){

  ASSERTARGS(list && gString);

  STRING_LIST *oldList;
  STRING_LIST *newList;

  oldList = *list;

  if(!oldList || !TYPE_STRING_LIST(oldList)){
	fprintf(stderr, "%s: Error invalid argument for parameter 'list' in prependGranularity().\n", progName);
	logMessage(fLogPtr, "Error invalid argument for parameter 'list' in prependGranularity().");
	return ztInvalidArg;
  }

  newList = initialStringList();
  if(!newList){
	fprintf(stderr, "%s: Error failed initialStringList().\n", progName);
	logMessage(fLogPtr, "Error failed initialStringList().");
	return ztMemoryAllocate;
  }

  char buffer[1024];
  char *oldStr, *newStr;

  ELEM *elem;

  elem = DL_HEAD(oldList);
  while(elem){

	oldStr = (char *)DL_DATA(elem);
	sprintf(buffer, "/%s%s", gString, oldStr);
	newStr = STRDUP(buffer);

	insertNextDL(newList, DL_TAIL(newList), (void *) newStr);

    elem = DL_NEXT(elem);
  }

  zapStringList((void **) &oldList);

  *list = newList;

  return ztSuccess;

} /* END prependGranularity() **/

#define IS_OK_SCHEME(x) ((strcmp(x, "http") == 0) || (strcmp(x, "https") == 0))

/* isSourceSupported(): checks source string using CURLU handle.
 * assumes handle was initialed with FULL URL source string.
 *
 * FULL URL string has set parts: scheme, host and path.
 *
 * furthermore function assumes LOWER CASE strings in above parts.
 *
 * return: ztSuccess when source is supported.
 *
 * Accepted sources:
 *
 * https://download.geofabrik.de/{AREA_COUNTRY}/[???]/{AREA_COUNTRY}-updates/
 * https://osm-internal.download.geofabrik.de/{AREA_COUNTRY}/[???]/{AREA_COUNTRY}-updates/
 *
 * https://planet.osm.org/replication/[minute|day|hour]/
 * https://planet.openstreetmap.org/replication/[minute|day|hour]/
 *
 * real examples for geofabrik server:
 *
 * https://download.geofabrik.de/north-america/us/arizona-updates/
 * https://download.geofabrik.de/north-america/us/california-updates/
 *
 * https://download.geofabrik.de/north-america/mexico-updates/
 *
 * https://download.geofabrik.de/africa/egypt-updates/
 * https://download.geofabrik.de/asia/india-updates/
 * https://download.geofabrik.de/europe/france-updates/
 *
 **************************************************************/

int isSourceSupported(char const *source, const CURLU *cParseHandle){

  CURLUcode   curluCode;
  CURLU *localHandle;



  /* all are ASSUMED in lower case **/
  char *srcInHandle = NULL; // source string retrieved from parser pointer
  char *scheme = NULL;
  char *host = NULL;
  char *path = NULL;

  ASSERTARGS(source && cParseHandle);

  localHandle = curl_url_dup(cParseHandle); /* clone parser handle! */

  if(! localHandle){
    fprintf(stderr, "%s: Error failed duplicate function: curl_url_dup() .\n", progName);
    return ztFailedLibCall;
  }

  //srcInHandle = getUrlStringCURLU(cParseHandle);
  srcInHandle = getUrlStringCURLU(localHandle);
  if(strcmp(source, srcInHandle) != 0){
    fprintf(stderr, "Error; curl parser handle does not have same string.\n");

    curl_url_cleanup(localHandle);
    return ztInvalidArg;
  }

  //curluCode = curl_url_get(cParseHandle, CURLUPART_SCHEME, &scheme, 0);
  curluCode = curl_url_get(localHandle, CURLUPART_SCHEME, &scheme, 0);
  if (curluCode != CURLUE_OK) {
    fprintf(stderr, "%s: Error failed curl_url_get() for scheme part.\n"
            "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));

    curl_url_cleanup(localHandle);

    return ztFailedLibCall;
  }

  //curluCode = curl_url_get(cParseHandle, CURLUPART_HOST, &host, 0);
  curluCode = curl_url_get(localHandle, CURLUPART_HOST, &host, 0);
  if (curluCode != CURLUE_OK) {
    fprintf(stderr, "%s: Error failed curl_url_get() for server part.\n"
            "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));

    curl_free(scheme);

    curl_url_cleanup(localHandle);

    return ztFailedLibCall;
  }

  //curluCode = curl_url_get(cParseHandle, CURLUPART_PATH, &path, 0);
  curluCode = curl_url_get(localHandle, CURLUPART_PATH, &path, 0);
  if ( curluCode != CURLUE_OK ) {
    fprintf(stderr, "%s: Error failed curl_url_get() for path part.\n"
            "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));

    curl_free(scheme);
    curl_free(host);

    curl_url_cleanup(localHandle);

    return ztFailedLibCall;
  }

  if( ! IS_OK_SCHEME(scheme)){
    fprintf(stderr, "%s: Error unsupported 'scheme' in source; scheme: <%s>\n", progName, scheme);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);

    curl_url_cleanup(localHandle);

    return ztInvalidArg;
  }

  if( ! isSupportedServer(host)){
    fprintf(stderr, "%s: Error unsupported 'server' in source; server: <%s>\n", progName, host);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);

    curl_url_cleanup(localHandle);

    return ztInvalidArg;
  }

  if(strstr(host, "planet") && ! isPlanetPath(path)){
    fprintf(stderr, "%s: Error invalid 'path' for server: <%s>; path: <%s>\n", progName, host, path);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);

    curl_url_cleanup(localHandle);

    return ztInvalidArg;
  }

  if(strstr(host, "geofabrik") && ! isGeofabrikPath(path)){
    fprintf(stderr, "%s: Error invalid 'path' for server: <%s>; path: <%s>\n", progName, host, path);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);

    curl_url_cleanup(localHandle);

    return ztInvalidArg;
  }

  curl_free(scheme);
  curl_free(host);
  curl_free(path);

  curl_url_cleanup(localHandle);

  return ztSuccess;

} /* END isSourceSupported() **/

int isSourceSupported_old(char const *source){

  CURLU       *srcCurluHandle;
  CURLUcode   curluCode;
//  int         result;

  /* all are ASSUMED in lower case **/
  //char *lowerSource;
  char *scheme = NULL;
  char *host = NULL;
  char *path = NULL;

  ASSERTARGS(source);
/*
  result = string2Lower(&lowerSource, source);
  if(!lowerSource){
    fprintf(stderr, "Error failed string2Lower() function.\n");
    return ztMemoryAllocate;
  }

  result = initialCurlSession();
  if (result != ztSuccess){
    fprintf(stderr, "Error failed initialCurlSession() function.\n");
    return result;

  }
*/
  /* get curl parse handle using our lowerSource **/
  //srcCurluHandle = initialURL(lowerSource);
  srcCurluHandle = initialURL(source);
  if (! srcCurluHandle){
    fprintf(stderr, "Error failed initialURL() function.\n");

    //free(lowerSource);
    //closeCurlSession();
    return ztFailedLibCall;
  }

  curluCode = curl_url_get(srcCurluHandle, CURLUPART_SCHEME, &scheme, 0);
  if (curluCode != CURLUE_OK) {
    fprintf(stderr, "%s: Error failed curl_url_get() for scheme part.\n"
            "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));

    //free(lowerSource);

    urlCleanup(srcCurluHandle);
    //closeCurlSession();

    return ztFailedLibCall;
  }

  curluCode = curl_url_get(srcCurluHandle, CURLUPART_HOST, &host, 0);
  if (curluCode != CURLUE_OK) {
    fprintf(stderr, "%s: Error failed curl_url_get() for server part.\n"
            "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));

    curl_free(scheme);
    //free(lowerSource);

    urlCleanup(srcCurluHandle);
    //closeCurlSession();

    return ztFailedLibCall;
  }

  curluCode = curl_url_get(srcCurluHandle, CURLUPART_PATH, &path, 0);
  if ( curluCode != CURLUE_OK ) {
    fprintf(stderr, "%s: Error failed curl_url_get() for path part.\n"
            "Curl error message: <%s>\n", progName,curl_url_strerror(curluCode));

    curl_free(scheme);
    curl_free(host);
    //free(lowerSource);

    urlCleanup(srcCurluHandle);
    //closeCurlSession();

    return ztFailedLibCall;
  }

//  if( ! isSupportedScheme(scheme)){
  if( ! IS_OK_SCHEME(scheme)){
    fprintf(stderr, "%s: Error unsupported 'scheme' in source; scheme: <%s>\n", progName, scheme);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);
    //free(lowerSource);

    urlCleanup(srcCurluHandle);
    //closeCurlSession();

    return ztInvalidArg;
  }

  if( ! isSupportedServer(host)){
    fprintf(stderr, "%s: Error unsupported 'server' in source; server: <%s>\n", progName, host);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);
    //free(lowerSource);

    urlCleanup(srcCurluHandle);
    //closeCurlSession();

    return ztInvalidArg;
  }

  if(strstr(host, "planet") && ! isPlanetPath(path)){
    fprintf(stderr, "%s: Error invalid 'path' for server: <%s>; path: <%s>\n", progName, host, path);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);
    //free(lowerSource);

    urlCleanup(srcCurluHandle);
    //closeCurlSession();

    return ztInvalidArg;
  }

  if(strstr(host, "geofabrik") && ! isGeofabrikPath(path)){
    fprintf(stderr, "%s: Error invalid 'path' for server: <%s>; path: <%s>\n", progName, host, path);

    curl_free(scheme);
    curl_free(host);
    curl_free(path);
    //free(lowerSource);

    urlCleanup(srcCurluHandle);
    //closeCurlSession();

    return ztInvalidArg;
  }

  curl_free(scheme);
  curl_free(host);
  curl_free(path);
  //free(lowerSource);

  urlCleanup(srcCurluHandle);
  //closeCurlSession();

  return ztSuccess;

} /* END isSourceSupported() **/




/* isSupportedScheme():
 *
 * returns TRUE or FALSE
 * assumes LOWER CASE string in scheme
 *
 ****************************************************************/

/*
int isSupportedScheme(const char *scheme){

  ASSERTARGS(scheme);

  char *supportedSchemes[] = {"http", "https", NULL};

  char **mover = supportedSchemes;

  while(*mover){

    if(strcmp(*mover, scheme) == 0){

      return TRUE;
    }

    mover++;
  }

  return FALSE;

}* END isSupportedScheme() **/

/* isSupportedServer():
 *
 * returns TRUE or FALSE
 * assumes LOWER CASE string in server
 *
 ****************************************************************/
int isSupportedServer(const char *server) {

  ASSERTARGS(server);

  /* Array of supported servers **/
  char *supportedServers[] = {
    "planet.openstreetmap.org",
    "planet.osm.org",
    "osm-internal.download.geofabrik.de",
    "download.geofabrik.de",
    NULL
  };

  char  **mover;

  for(mover = supportedServers; *mover; mover++){

    if(strcmp(*mover, server) == 0){

      return TRUE;
    }
  }

  return FALSE;

} /* END isSupportedServer() **/

/* isPlanetPath():
 *
 * returns TRUE or FALSE
 * assumes LOWER CASE string in path
 *
 * https://planet.osm.org/replication/[minute|day|hour]/
 * https://planet.openstreetmap.org/replication/[minute|day|hour]/
 *
 ****************************************************************/
int isPlanetPath(const char *path){

  ASSERTARGS(path);

  int  result;
  char *myPath;

  char *entry1 = "replication";
  char *entry2Array[] = {"day", "hour", "minute", NULL};
  char **mover;

  char *myLastEntry, *myFirstEntry;

  result = isGoodDirName(path); /* applies strict rules including rejecting
                                   space character and multiple slashes **/
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isGoodDirName() in isPlanetPath() for 'path': <%s>\n",
            progName, path);

    return FALSE;
  }

  myPath = STRDUP(path); /* function allocates memory, exits program on failure! **/

  myLastEntry = lastOfPath(myPath); /* lastOfPath() BEFORE strtok() **/

  if(!myLastEntry){
    fprintf(stderr, "%s: Error failed lastOfPath() in isPlanetPath().\n", progName);
    free(myPath);

    return FALSE;
  }

  myFirstEntry = strtok(myPath, "/");

  if(!myFirstEntry){
    fprintf(stderr, "%s: Error failed strtok() in isPlanetPath().\n", progName);
    free(myPath);
    free(myLastEntry);

    return FALSE;
  }

  if(strcmp(myFirstEntry, entry1) != 0){
    fprintf(stderr, "%s: Error invalid first entry of path in isPlanetPath().\n", progName);
    free(myPath);
    free(myLastEntry);

    return FALSE;
  }

  mover = entry2Array;

  while(*mover){
    if(strcmp(*mover, myLastEntry) == 0){
      free(myPath);
      free(myLastEntry);

      return TRUE;
    }
    mover++;
  }

  free(myPath);
  free(myLastEntry);

  fprintf(stderr, "%s: Error invalid last entry of path in isPlanetPath().\n", progName);

  return FALSE;

} /* END isPlanetPath() **/

/* isGeofabrikPath():
 *
 * returns TRUE or FALSE
 * assumes LOWER CASE string in path
 *
 ****************************************************************/
int isGeofabrikPath(const char *path){

  ASSERTARGS(path);

  int  result;
  char *myPath;

  /* first entry in path must be one in list **/
  char *entry1Array[] = {
    "africa", "antarctica",
    "asia", "australia-oceania",
    "central-america", "europe",
    "north-america", "south-america",
    NULL};

  /* last entry in path must end with '-updates' string **/
  char *lastEntryMark = "-updates";

  char *myFirstEntry;
  char *myLastEntry;


  result = isGoodDirName(path); /* applies strict rules including rejecting
                                   space character and multiple slashes **/
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isGoodDirName() in isGeofabrikPath() for 'path': <%s>\n",
            progName, path);
    return FALSE;
  }

  myPath = STRDUP(path); /* function allocates memory **/

  myLastEntry = lastOfPath(myPath); /* function allocates memory **/

  if(!myLastEntry){
    fprintf(stderr, "%s: Error failed lastOfPath() in isGeofabrikPath().\n", progName);
    free(myPath);

    return FALSE;
  }

  char *lastDash;

  lastDash = strrchr(myLastEntry, '-');

  if(!lastDash){
    fprintf(stderr, "%s: Error failed to find dash in last entry in isGeofabrikPath().\n", progName);
    free(myPath);
    free(myLastEntry);

    return FALSE;
  }

  if(strcmp(lastEntryMark, lastDash) != 0){
    fprintf(stderr, "%s: Error last entry does not end with '-updates' in isGeofabrikPath().\n", progName);
    free(myPath);
    free(myLastEntry);

    return FALSE;
  }

  myFirstEntry = strtok(myPath, "/");
  if(!myFirstEntry){
    fprintf(stderr, "%s: Error failed strtok() in isGeofabrikPath().\n", progName);
    free(myPath);
    free(myLastEntry);

    return FALSE;
  }

  char **mover = entry1Array;

  for(; *mover; mover++){
    if(strcmp(*mover, myFirstEntry) == 0){
      free(myPath);
      free(myLastEntry);

      return TRUE;
    }
  }

  free(myPath);
  free(myLastEntry);

  fprintf(stdout, "%s: Error first entry is NOT a continent in isGeofabrikPath().\n", progName);

  return FALSE;

} /* END isGeofabrikPath() **/

