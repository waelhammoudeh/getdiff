/*
============================================================================
Name        : getdiff.c
Author      : Wael Hammoudeh
Date	: Feb. 7/2022
Version     :
Copyright   : Your copyright notice
Description : program to download differ files from https://osm-internal.download.geofabrik.de/
to update overpass database with limited area installation.
============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "getdiff.h"
#include "util.h"
#include "ztError.h"
#include "parse.h"
#include "configure.h"
#include "usage.h"
#include "cookie.h"
#include "network.h"
#include "curl_func.h"
#include "dList.h"
#include "fileio.h"
#include "parseHtml.h"

/* those names can not be changed by user; so I use defines for them */
#define	CONF_NAME                "getdiff.conf"
#define	CONF_DEFAULT           "/etc/getdiff.conf"
#define	LOG_NAME					"getdiff.log"
#define	INDEX_NAME				"index.html"
#define	INDEX_LIST_FILE			"indexList.txt"
#define  START_FILE_NAME		"start_id"
#define	DOWNLOAD_ENTRY		"Downloads"
#define  WORK_ENTRY				"getdiff"
#define	DIFF_ENTRY		            "diff"
#define	TEST_SITE					"www.geofabrik.de"


static DL_ELEM* findElemSubstring (DL_LIST *list, char *subStr);

/* program name is global, used in error msgs */
char		*progName = NULL;
int		flagVerbose = 0;

int main(int argc, char *argv[]){

    int        result;
    int		 haveReqArgs;
    int        haveConfArgs;
    char    *homeDir;
    char    *defDest;
    char    *defConf;
    char    *executableDir;	// self executable directory
    char    *altConf;  // 2nd choice configure in our executable directory
    char    tmpBuf[PATH_MAX];
    char    *someString;
    char    *scriptName = "oauth_cookie_client.py";
    char    *jsonName = "settings.json";
    char    *cookieName = "geofabrikCookie.text";

    SETTINGS        *mySettings;

    CONF_ENTRY 	confEntries[] = {
            {"USER", NULL, NAME_CT, 1},
            {"PASSWD", NULL, NAME_CT, 2},
            {"SOURCE", NULL, URL_CT, 3},
            {"DIRECTORY", NULL, DIR_CT, 4},
			{"TEST_SITE", NULL, NAME_CT, 5},
			{"BEGIN", NULL, NAME_CT, 6},
			{"VERBOSE", NULL, BOOL_CT, 7},
			{"NEWER_FILE", NULL, NAME_CT, 8},
            {NULL, NULL, 0, 0}
    };

    int		numConfFound = 0; // for readConfigure()
    int		connected;
    char		*ipBuf;
    char		*testServer;
    FILE		*fLogPtr;
    char		*currentTime;

    COOKIE	*myCookie = NULL;
    DL_LIST	*pageList; // server listing
    DL_LIST	*sessionDL; // current session downloaded files

    FILE		*indexFilePtr;

    CURLU	*curlParseHandle;
    char		*server, *path;

    char		*internalServer = "osm-internal.download.geofabrik.de";
    char		*publicServer = "download.geofabrik.de";
    CURL 	*myDwnldHandle;
    char		*secTok;

    char		*startStr = NULL; /* read string by readStart_Id() */
    char		*currentStart;
    DL_ELEM	*startElem = NULL;
    DL_ELEM	*elem;
    int			iCount;
    char			*filename;
    FILE			*filePtr;

    /* set some defaults for program */
    progName = lastOfPath(argv[0]);

    if (argc < 2){

    	fprintf(stderr, "%s: Error missing argument.\n\n", progName);
    	shortHelp();
    	return ztMissingArgError;
    }

    testServer = strdup(TEST_SITE); /* bad idea? check internet connection */

    /* prepare defaults we may need:
    *    defDest : {USER-HOME}/Downloads/getdiff/
    *    defConf : /etc/getdiff.conf
    *    altConf : {EXEC-DIR}/getdiff.conf
    *******************************************/
    homeDir = getHome();

    if(IsSlashEnding(homeDir))
        sprintf(tmpBuf, "%s%s/%s", homeDir, DOWNLOAD_ENTRY, WORK_ENTRY);
    else
        sprintf(tmpBuf, "%s/%s/%s", homeDir, DOWNLOAD_ENTRY, WORK_ENTRY);

    defDest = strdup(tmpBuf);

    defConf = strdup(CONF_DEFAULT);

    executableDir = get_self_executable_directory (); // where we live

    if (IsSlashEnding(executableDir))
        sprintf (tmpBuf, "%s%s", executableDir, CONF_NAME);
    else
        sprintf (tmpBuf, "%s/%s", executableDir, CONF_NAME);

    altConf = strdup(tmpBuf);

    mySettings = (SETTINGS *) malloc (sizeof(SETTINGS));
    if ( ! mySettings ){
        fprintf (stderr, "%s: Error allocating memory.\n", progName);
        return ztMemoryAllocate;
    }
    memset(mySettings, 0, sizeof(SETTINGS));

    /* mySettings is empty going into parseCmdLine() */

    result = parseCmdLine(mySettings, argc, argv);
    if (result != ztSuccess){
        fprintf(stderr, "%s: Error malformed command line! parseCmdLine() returned error. Exiting.\n", progName);
        return result;
    }

    /* call readConfigure() if NOT ALL options were specified on command line
     * first figure out configuration file location */

    haveReqArgs = (mySettings->src != NULL);

    haveConfArgs = ( haveReqArgs &&
    		                    (mySettings->dst != NULL) &&
								(mySettings->tstSrvr != NULL) &&
								(mySettings->start != NULL) );

    /* we check usr and psswd after server setting */

    /* if user did NOT specify configure file on command line, try defConf then altConf.
     * note that we do not change it if it was set coming out of parseCmdLine(). */
    if ( ! mySettings->conf && (IsArgUsableFile(defConf) == ztSuccess) )

        mySettings->conf = defConf;

    else if (! mySettings->conf && (IsArgUsableFile(altConf) == ztSuccess) )

        mySettings->conf = altConf;

    if ( ! mySettings->conf) /* did not get set, write warning */

        fprintf(stdout, "%s: Warning not using any configuration file. No conf settings.\n", progName);

    if ( mySettings->conf && ( ! haveConfArgs) ){

    	/* read configure file if NOT all settings were provided on command line */

        result = readConfigure(confEntries, &numConfFound, mySettings->conf);
        if (numConfFound == 0)
        	fprintf(stdout, "%s: Warning ZERO settings in configuration file: %s\n", progName, mySettings->conf);

        if ( result != ztSuccess){
            fprintf(stderr, "ERROR returned by readConfigure(). File: [%s]\n", mySettings->conf);
            fprintf(stderr, "Error message: %s\n\n", code2Msg(result));
            fprintf(stderr, "Line NUMBER in configure file with error is: %d\n\n", confLN); //readConfigure() sets confLN
            return result;
        }

        result = updateSettings(mySettings, confEntries);
        if (result != ztSuccess){

            fprintf (stderr, "%s: Error returned from updateSettings().\n", progName);
            return result;
        }
    }

    /* set flagVerbose if in use */
    if (mySettings->verbose){
    	flagVerbose = 1;
    }

    if ( ! mySettings->src ){

        fprintf (stderr, "%s: Error missing required source URL.\n", progName);
        return ztMissingArgError;

    }

    /* usr and psswd settings depend on the server we connect to.
     * figure out server in use. initialCurlSession() first. */
    initialCurlSession();
    curlParseHandle = initialURL(mySettings->src);
    if ( ! curlParseHandle ){
    	fprintf (stderr, "%s: Error return from initialURL().\n", progName);
    	return ztUnknownError;
    }

    result = curl_url_get(curlParseHandle, CURLUPART_HOST, &server, 0);
    if ( result != CURLUE_OK ) {
      fprintf(stderr, "%s: Error failed curl_url_get() server.\n"
    		  "Curl error msg: <%s>\n", progName,curl_url_strerror(result));
      return result;
    }

    /* path part is needed further down, get it now */
    result = curl_url_get(curlParseHandle, CURLUPART_PATH, &path, 0);
    if ( result != CURLUE_OK ) {
      fprintf(stderr, "%s: Error failed curl_url_get() path.\n"
    		  "Curl error msg: <%s>\n", progName,curl_url_strerror(result));
      return result;
    }

	/* only accept internalServer or publicServer from geofabrik */
	if ( (strcasecmp(server, internalServer) != 0) &&
		  (strcasecmp(server, publicServer) != 0) ) {

		fprintf(stderr, "%s: Error: invalid server name from source url: {%s}\n"
				"Valid servers are from \"geofabrik.de\" namely: <%s> and <%s>.\n",
				progName, server, internalServer, publicServer);
		return ztInvalidArg;
	}

	/* usr and psswd are required arguments for internalServer */
	if ( (strcasecmp(server, internalServer) == 0) ){

	    haveReqArgs = (mySettings->usr && mySettings->psswd);

		if ( ! haveReqArgs ){

			fprintf(stderr, "%s: Error missing required argument(s):\n",
					progName);
			if (!mySettings->usr)
				fprintf(stderr, "\tUser name\n");

			if (!mySettings->psswd)
				fprintf(stderr, "\tpassword\n");

			fprintf(stderr, "OSM user name and password are required to use the"
					" internal server.\n"
					"Note collected settings are as follows:\n");
			printSettings(mySettings);
			return ztMissingArgError;
		}

	} /* end if ( (strcasecmp(server, internalServer) == 0) ) */

	/* done collecting and checking input - we need internet connection here */

    if (mySettings->tstSrvr)

    	testServer = mySettings->tstSrvr;

    result =  checkURL (testServer, NULL, &ipBuf);
    connected = (result == ztSuccess);
    if ( ! connected ){
    	fprintf(stderr, "%s: Error fatal! Not connected to Internet. This program "
    			"requires Internet connection to work.\n", progName);
    	return ztNoConnError;
    }

    /* fill other members in mySettings structure.
     * NOTE: we only set names below; nothing is created
     * until further down. */

    if (! mySettings->workDir)

    	mySettings->workDir = defDest;

    /* we always work under "getdiff" directory name!
     * add "getdiff" entry if it is not the last one. */

    someString = lastOfPath(mySettings->workDir);
    if (strcmp(someString, WORK_ENTRY) != 0){

    	if(IsSlashEnding(mySettings->workDir))
            sprintf(tmpBuf, "%s%s/", mySettings->workDir, WORK_ENTRY);
        else
            sprintf(tmpBuf, "%s/%s/",mySettings->workDir, WORK_ENTRY);

    	mySettings->workDir = strdup (tmpBuf);
    }

    /* set "diff" directory under workDir */
    if(IsSlashEnding(mySettings->workDir))
        sprintf(tmpBuf, "%s%s/", mySettings->workDir, DIFF_ENTRY);
    else
        sprintf(tmpBuf, "%s/%s/",mySettings->workDir, DIFF_ENTRY);

    mySettings->dst = strdup (tmpBuf);

    if(IsSlashEnding(mySettings->workDir))
        sprintf(tmpBuf, "%s%s", mySettings->workDir, INDEX_NAME);
    else
        sprintf(tmpBuf, "%s/%s",mySettings->workDir, INDEX_NAME);

    mySettings->htmlFile = strdup(tmpBuf);

    if(IsSlashEnding(mySettings->workDir))
        sprintf(tmpBuf, "%s%s", mySettings->workDir, LOG_NAME);
    else
        sprintf(tmpBuf, "%s/%s",mySettings->workDir, LOG_NAME);

    mySettings->logFile = strdup(tmpBuf);

    if(IsSlashEnding(mySettings->workDir))
        sprintf(tmpBuf, "%s%s", mySettings->workDir, INDEX_LIST_FILE);
    else
        sprintf(tmpBuf, "%s/%s",mySettings->workDir, INDEX_LIST_FILE);

    mySettings->indexListFile = strdup(tmpBuf);

    if(IsSlashEnding(mySettings->workDir))
        sprintf(tmpBuf, "%s%s", mySettings->workDir, START_FILE_NAME);
    else
        sprintf(tmpBuf, "%s/%s",mySettings->workDir, START_FILE_NAME);

    mySettings->startFile = strdup(tmpBuf);

    memset(tmpBuf, 0, sizeof(tmpBuf));

    /* if internalServer set: scriptFile, jsonFile and cookieFile names */
    if ( (strcasecmp(server, internalServer) == 0) ){

        if(IsSlashEnding(mySettings->workDir))
            sprintf(tmpBuf, "%s%s", mySettings->workDir, scriptName);
        else
            sprintf(tmpBuf, "%s/%s",mySettings->workDir, scriptName);

        mySettings->scriptFile = strdup(tmpBuf);

        if(IsSlashEnding(mySettings->workDir))
            sprintf(tmpBuf, "%s%s", mySettings->workDir, jsonName);
        else
            sprintf(tmpBuf, "%s/%s",mySettings->workDir, jsonName);

        mySettings->jsonFile = strdup(tmpBuf);

        if(IsSlashEnding(mySettings->workDir))
            sprintf(tmpBuf, "%s%s", mySettings->workDir, cookieName);
        else
            sprintf(tmpBuf, "%s/%s",mySettings->workDir, cookieName);

        mySettings->cookieFile = strdup(tmpBuf);

    }

    /* make directories (workDir and dst) - if they do not exist */
	result = myMkDir(mySettings->workDir);
	if (result != ztSuccess){

		fprintf(stderr, "%s: Error failed to create work directory.\n", progName);
		return result;
	}

	result = myMkDir(mySettings->dst);
	if (result != ztSuccess){

		fprintf(stderr, "%s: Error failed to create destination directory.\n", progName);
		return result;
	}

    /* read start_id file if it exist - before looking at command line or configure
     * begin argument. This is so unusual; ignoring command line in favor of
     * having startFile in our working directory! */
    if (IsArgUsableFile(mySettings->startFile) == ztSuccess) {

    	result = readStart_Id (&startStr, mySettings->startFile);
    	if (result != ztSuccess) {

    		fprintf (stderr, "%s: Error returned from readStart_id().\n", progName);
    		return result;
    	}

    	mySettings->start = startStr; /* replace start settings with value from start_id */
    	currentStart = startStr;
    }

    else if (mySettings->start) {

    	currentStart = mySettings->start;
    }

    else { /* we do nothing without either above options */

    	fprintf (stdout, "%s: start_id file was not found, no begin option was used. "
    			"Please provide file number to start download from.\n"
    			"See sorted list file ...\n", progName);
    	return ztSuccess;
    }

	/* open - create log file & write started @ current time */
	errno = 0;
	fLogPtr = fopen (mySettings->logFile, "a");
	if ( fLogPtr == NULL){
		fprintf (stderr, "%s: Error could not open log file! <%s>\n",
				     progName, mySettings->logFile);
		printf("System error message: %s\n\n", strerror(errno));
	    return ztOpenFileError;
	}

	currentTime = formatC_Time ();
	fprintf (fLogPtr, "%s started at: %s\n", progName, currentTime);

	if (flagVerbose)

		printSettings(mySettings);

	if ((strcasecmp(server, publicServer) == 0) ){

		logMessage(fLogPtr, "Geofabrik Public Server is in use.");
	}
	else	if ( (strcasecmp(server, internalServer) == 0) ){

		logMessage(fLogPtr, "Geofabrik internal server is in use.");

		myCookie = (COOKIE *) malloc(sizeof(COOKIE));
		if ( ! myCookie ){
			fprintf(stderr, "%s: Error allocating memory.\n", progName);
			return ztMemoryAllocate;
		}

		if (IsArgUsableFile (mySettings->cookieFile) != ztSuccess){

			result = getCookieFile(mySettings);
			if (result != ztSuccess){

				fprintf (stderr, "%s: Error we failed to retrieve login cookie :( No cookie! Maybe faulty software?\n"
		                " But check user name and password anyway, please.\n", progName);
		        return ztUnknownError;
			}
		}

		memset(myCookie, 0, sizeof(COOKIE));
		result = parseCookieFile(myCookie, mySettings->cookieFile);
		if (result != ztSuccess){

			fprintf(stderr, "%s: Error returned from parseCookieFile() function.\n"
					"Like me, this code is not perfect. Keep in mind cookie file format may "
					"have changed and I missed the memo.\n", progName);
			return result;
		}

	    result = isExpiredCookie(myCookie);
	    if (result == FALSE)

	    	fprintf(stdout, "%s: Using existing cookie with non-expired date.\n", progName);

	    else {

	        fprintf(stdout, "%s: Existing cookie has expired, retrieving new cookie ...\n", progName);

	        result = getCookieFile(mySettings);
			if (result != ztSuccess){

				fprintf (stderr, "%s: Error we failed to retrieve login cookie :( No cookie! Maybe faulty software?\n"
		                " But check user name and password anyway, please.\n", progName);
		        return ztUnknownError;
			}

			memset(myCookie, 0, sizeof(COOKIE));
			result = parseCookieFile(myCookie, mySettings->cookieFile);
			if (result != ztSuccess){

				fprintf(stderr, "%s: Error returned from parseCookieFile() function.\n", progName);
				return result;
			}
		    if (isExpiredCookie(myCookie) != FALSE){

		    	fprintf(stderr, "%s: Error expired date detected in newly retrieved cookie.\n", progName);
		    	return ztUnknownError;
		    }
	    }

	} /* end if (internalServer) */

	if (myCookie && myCookie->token)

		secTok = myCookie->token;

	else

		secTok = NULL;

	/* from here down - both servers are the same */
	/* download index.html page */

    errno = 0;
    indexFilePtr = fopen(mySettings->htmlFile, "w+"); /* writing + reading : verify HTML without another open call. skipped! */
    if ( ! indexFilePtr ){

    	fprintf (stderr, "%s: Error could not create file! <%s>\n",
                progName, mySettings->htmlFile);
        printf("System error message: %s\n\n", strerror(errno));
        return ztCreateFileErr;
    }

    myDwnldHandle = initialDownload (curlParseHandle, secTok);
    if ( ! myDwnldHandle ){
    	fprintf(stderr, "%s: Error returned from initialDownload().\n", progName);
    	return ztUnknownError;
    }

    sprintf (tmpBuf, "Getting the HTML index page for source: %s", mySettings->src);
    fprintf(stdout, "%s\n", tmpBuf);
    logMessage(fLogPtr, tmpBuf);

    result = performDownload (indexFilePtr, myDwnldHandle);
    if (result != ztSuccess){
    	fprintf(stderr, "%s: Error returned from performDownload() for file: <%s>."
    			" Please check the source URL.\n", progName, mySettings->htmlFile);
    	return result;
    }

    fflush(indexFilePtr);
    fclose(indexFilePtr);

    sprintf(tmpBuf, "Wrote HTML page file to: <%s>", mySettings->htmlFile);
    fprintf(stdout, "%s: %s\n", progName, tmpBuf);
    logMessage (fLogPtr, tmpBuf);

	/* parse index.html file - result is returned in pageList */
    pageList = (DL_LIST *) malloc(sizeof(DL_LIST));
    if ( ! pageList ){
    	fprintf(stdout, "%s: Error allocating memory.\n", progName);
    	return ztMemoryAllocate;
    }
    initialDL(pageList, zapString, NULL);

    result = parseIndexPage (pageList, mySettings->htmlFile);
    if (result != ztSuccess){
    	fprintf(stderr, "%s: Error returned from parseListPage() function.\n", progName);
    	return result;
    }

    /* write parsed sorted list to file "indexList.txt" */
    result = strList2File(mySettings->indexListFile, pageList);
    if (result != ztSuccess){
    	fprintf(stderr, "%s: Error returned from strList2File() function.\n", progName);
    	return result;
    }
    else {

    	fprintf(stdout, "%s: Wrote current sorted list to: < %s >\n",
    			progName, mySettings->indexListFile);
    }

    /* currentStart is set at this point; first use from user, start_id
     * file is used to get currentStart if it exists.
     * 6 lines comment here. 2 lines above are rephrased in 3 lines below!
     * we write last downloaded differ number in our start_id file;
     * if currentStart came from that file move to next differ.
     * First use will have no start_id file, user begin is what we use.
     * ***************************************************************/

    startElem = findElemSubstring (pageList, currentStart);
    if ( ! startElem ){

    	fprintf(stderr, "%s: Error could not find element with start id "
    			"string <%s> in current differs list!\n", progName, currentStart);
    	/* return ztGotNull; */
    	return ztNotFound;
    }

   /* DL_IS_TAIL(element) - 2 files always: 288.osc.gz and 288.state.txt
    findElemSubstring() gets the first one, if we are at the end of the list,
    then there is nothing to get. */

    if (IsArgUsableFile(mySettings->startFile) == ztSuccess) {

    	sprintf(tmpBuf, "Last downloaded file was <%s>", currentStart);
    	fprintf( stdout, "%s: %s\n", progName, tmpBuf);
    	logMessage(fLogPtr, tmpBuf);

    	if ( DL_IS_TAIL(DL_NEXT(startElem)) ){

    		sprintf(tmpBuf, "Last element in current sorted list was last downloaded file. Nothing new to get.");
    		fprintf( stdout, "%s: %s\n", progName, tmpBuf);
    		logMessage(fLogPtr, tmpBuf);
    	}

    	// move startElem to next differ
    	startElem = DL_NEXT(DL_NEXT(startElem));
    }
    else {

    	sprintf(tmpBuf, "No \"start_id\" file. First use or was deleted. Starting at: <%s>", currentStart);
		fprintf( stdout, "%s: %s\n", progName, tmpBuf);
		logMessage(fLogPtr, tmpBuf);
    }


    if (startElem){

    	fprintf(stdout, "Starting download with file number: <%s>\n", (char *) DL_DATA(startElem));
    }

    // initial sessionDL list
    sessionDL = (DL_LIST *) malloc(sizeof(DL_LIST));
    if ( ! sessionDL ){
    	fprintf(stdout, "%s: Error allocating memory.\n", progName);
    	return ztMemoryAllocate;
    }
    initialDL(sessionDL, zapString, NULL);

	/* curlParseHandle has the path PART for source; see above when initialed:
	 *      curlParseHandle = initialURL(mySettings->src);
	 * pageList has filenames for differs SORTED.
	 * myDwnldHandle is CURL easy connected to / initialed with curlParseHandle
	 * startElem : the element in pageList to start download from
	 */
	elem = startElem;
	iCount = 0;
	while (elem){

		filename = (char *) DL_DATA(elem);

		if (IsSlashEnding(path))
			sprintf(tmpBuf, "%s%s", path, filename);
		else
			sprintf(tmpBuf, "%s/%s", path, filename);

		/* replace the path PART with that includes filename. TODO: check return */
		curl_url_set (curlParseHandle, CURLUPART_PATH, tmpBuf, 0);

		/* open file where we save our downloads */
		if (IsSlashEnding (mySettings->dst))
			sprintf(tmpBuf, "%s%s", mySettings->dst, filename);
		else
			sprintf(tmpBuf, "%s/%s", mySettings->dst, filename);

	    errno = 0;
	    filePtr = fopen(tmpBuf, "w");
	    if ( ! filePtr ){

	    	fprintf (stderr, "%s: Error could not create file! <%s>\n",
	                progName, tmpBuf);
	        printf("System error message: %s\n\n", strerror(errno));
	        return ztCreateFileErr;
	    }

	    fprintf(stdout, "Getting file: %s\n", filename);
	    result = performDownload (filePtr, myDwnldHandle);
	    if (result != ztSuccess){
	    	fprintf(stderr, "%s: Error returned from performDownload() for file: <%s>."
	    			" Please check the source URL.\n", progName, tmpBuf);
	    	return result;
	    }

	    fflush(filePtr);
	    fclose(filePtr);

	    sprintf(tmpBuf, "downloaded file: %s", filename);
		fprintf( stdout, "%s: %s\n", progName, tmpBuf);
		logMessage(fLogPtr, tmpBuf);

		/* insert downloaded filename into sessionDL as last element - source is sorted */
		result = insertNextDL (sessionDL, DL_TAIL(sessionDL), (void *) filename);
		if (result != ztSuccess){
			fprintf (stderr, "%s: Error inserting filename into sessionDL.\n", progName);
			return result;
		}

	    iCount++;

		if ( (iCount / 2) == 30){ /* get & keep files in PAIRS */

			fprintf(stdout, "%s: Warning maximum download reached! "
					"Please wait some time before trying again, thank you.\n", progName);
			break;
		}

		elem = DL_NEXT(elem);
	}

	if ( iCount ){ /* downloaded some files - update start_id file. after each file??
	                         append newer files list to newerFile if set */

		filename = (char *) DL_DATA(DL_TAIL(sessionDL));

		memset(tmpBuf, 0, sizeof(tmpBuf));

		snprintf(tmpBuf, 4, "%s", filename);

		/* start_id file gets the number from file in the TAIL element */
		writeStart_Id (tmpBuf, mySettings->startFile);

		if (mySettings->newerFile)

			//writeNewerFile (mySettings->newerFile, startElem, pageList);
			writeNewerFile (mySettings->newerFile, DL_HEAD(sessionDL), sessionDL);
	}

	currentTime = formatC_Time();
	fprintf (fLogPtr, "---------- Done at: %s ----------\n\n", currentTime);

	fflush(fLogPtr);
	fclose(fLogPtr);

	/* curl cleanup */
	easyCleanup(myDwnldHandle);
	curl_free(server);
	curl_free(path);
	urlCleanup(curlParseHandle);
	closeCurlSession();

	destroyDL(pageList);

    return ztSuccess;

} // END main()

void printSettings(SETTINGS *settings){

    ASSERTARGS(settings);

    fprintf(stdout, "printSettings() : Those are the current settings:\n\n");

    if (settings->usr)
        fprintf(stdout, "OSM USER is: <%s>\n", settings->usr);
    else
        fprintf(stdout, "OSM USER is NOT set.\n");

    if (settings->psswd)
        fprintf(stdout, "Password is: <%s>\n", "xxxxxxxxxx");
    else
        fprintf(stdout, "Password is NOT set.\n");

    if (settings->conf)
        fprintf(stdout, "Configuration File is: <%s>\n", settings->conf);
    else
        fprintf(stdout, "Configuration File is NOT set.\n");

    if (settings->src)
        fprintf(stdout, "Source URL is: <%s>\n", settings->src);
    else
        fprintf(stdout, "Source is NOT set.\n");

    if (settings->workDir)
    	fprintf(stdout, "Work directory is: <%s>\n", settings->workDir);
    else
    	fprintf(stdout, "Work directory is NOT set.\n");

    if (settings->dst)
        fprintf(stdout, "Destination is: <%s>\n", settings->dst);
    else
        fprintf(stdout, "Destination is NOT set.\n");

    if (settings->startFile)
    	fprintf(stdout, "Start File Name is: <%s>\n", settings->startFile);
    else
    	fprintf(stdout, "Start File Name is NOT set.\n");

    if (settings->start)
    	fprintf(stdout, "Begin with is: <%s>\n", settings->start);
    else
    	fprintf(stdout, "Begin is NOT set.\n");

    if (settings->cookieFile)
        fprintf(stdout, "Cookie File is: <%s>\n", settings->cookieFile);
    else
        fprintf(stdout, "Cookie File is NOT set.\n");

    if (settings->scriptFile)
        fprintf(stdout, "Script File is: <%s>\n", settings->scriptFile);
    else
        fprintf(stdout, "Script File is NOT set.\n");

    if (settings->jsonFile)
            fprintf(stdout, "JSON File is: <%s>\n", settings->jsonFile);
        else
            fprintf(stdout, "JSON File is NOT set.\n");

    if (settings->logFile)
    	fprintf(stdout, "LOG File is: <%s>\n", settings->logFile);
    else
    	fprintf(stdout, "LOG File is NOT set.\n");

    if (settings->htmlFile)
    	fprintf(stdout, "HTML File is: <%s>\n", settings->htmlFile);
    else
    	fprintf(stdout, "HTML File is NOT set.\n");

    if (settings->indexListFile)
    	fprintf(stdout, "Sorted LIST File is: <%s>\n", settings->indexListFile);
    else
    	fprintf(stdout, "Sorted LIST File is NOT set.\n");

    if (settings->verbose)
    	fprintf(stdout, "Verbose is on: <%s>\n", settings->verbose);
    else
    	fprintf(stdout, "Verbose is NOT set.\n");

    if (settings->newerFile)
    	fprintf(stdout, "Newer File name is set to: <%s>\n", settings->newerFile);
    else
    	fprintf(stdout, "Newer File name is NOT set.\n");

    fprintf (stdout, "\n");

    return;
}

/* updateSettings(): configure entry values are used only if NOT set in settings -
 * from command line - AND entry values are checked first.
 */
int updateSettings (SETTINGS *settings, CONF_ENTRY confEntries[]){

    CONF_ENTRY	    *moverEntry;

    ASSERTARGS (settings && confEntries);

    /* use configure setting only when not specified on command line */
    moverEntry = confEntries;
    while (moverEntry->name){

        switch (moverEntry->index){

        case 1: // USER

            if ( ! settings->usr && moverEntry->value){

            	if ( strlen(moverEntry->value) > MAX_NAME_LENGTH ) {

            		fprintf (stderr, "%s: updateSettings() Error user name value "
            				"is longer than 64 characters.\n", progName);
                    return ztInvalidArg;
            	}

            	settings->usr = strdup (moverEntry->value);
            }
            break;

        case 2: // PASSWD

            if ( ! settings->psswd && moverEntry->value){

            	if ( strlen(moverEntry->value) > MAX_NAME_LENGTH ) {

            		fprintf (stderr, "%s: updateSettings() Error user password value "
            				"is longer than 64 characters.\n", progName);
                    return ztInvalidArg;
            	}

            	settings->psswd = strdup (moverEntry->value);
            }
            break;

        case 3: // SOURCE

            if ( ! settings->src && moverEntry->value) {

            	if ( ! isGoodURL(moverEntry->value)){

            		fprintf (stderr, "%s: updateSettings() Error source value is"
            				" not good URL string.\n", progName);
                    return ztInvalidArg;
            	}

                settings->src = strdup (moverEntry->value);
            }
            break;

        case 4: // DEST_DIR

            if ( ! settings->workDir && moverEntry->value){

                if (moverEntry->value[0] == '~' || moverEntry->value[0] == '.') {
                    fprintf(stderr, "%s: Error this program does NOT handle path expansion, configure DEST_DIR: <%s>\n"
                            "Please use REAL FULL path. FYI on the command line expansion is done by the shell.\n",
                            progName, moverEntry->value);
                    return ztBadParam;
                }

                if ( IsGoodDirectoryName(moverEntry->value) )

                	settings->workDir = strdup (moverEntry->value);

                else {

            		fprintf (stderr, "%s: updateSettings() Error bad directory name for destination "
            				"value: [%s].\n", progName,moverEntry->value);
                    return ztInvalidArg;
            	}
            }
            break;

        case 5: // TEST_SITE

        	if ( ! settings->tstSrvr && moverEntry->value)

        		if (strlen(moverEntry->value) < 33)

        			settings->tstSrvr = strdup (moverEntry->value);

        	break;

        case 6: // BEGIN

        	if ( ! settings->start && moverEntry->value){

				if (strspn(moverEntry->value, "1234567890") == strlen(moverEntry->value) &&
					strlen(moverEntry->value) == 3 ){

					settings->start = strdup (moverEntry->value);
				}
				else {
            		fprintf (stderr, "%s: updateSettings() Error, invalid configure \"BEGIN\" argument. "
            				"Please provide only the 3 digits from the file name.\n"
            				": [%s].\n", progName, moverEntry->value);
                    return ztInvalidArg;
				}
        	}

        	break;

        case 7: // VERBOSE - we only turn it on!

        	if ( ! settings->verbose && moverEntry->value){

        		if ( (strcasecmp(moverEntry->value, "true") == 0) ||
        			 (strcasecmp(moverEntry->value, "on") == 0) ||
        			 (strcmp(moverEntry->value, "1") == 0))

        			settings->verbose = "YES";
        	}

        	break;

        case 8: // NEWER_FILE

        	if ( ! settings->newerFile && moverEntry->value){

				if ( ! IsGoodFileName (moverEntry->value) ){

					fprintf (stderr, "%s: Error bad file name specified for NEWER_FILE value "
            				": [%s].\n", progName, moverEntry->value);
                    return ztInvalidArg;
				}

				settings->newerFile = strdup (moverEntry->value);

        	}

        	break;

        default:

            break;
        } // end switch

            moverEntry++;

    } // end while()

    return ztSuccess;

} // END updateSettings()

int readStart_Id (char **dest, char *filename){

	FILE		*fPtr;
	char		line[1024] = {0};
	int		count = 0;
	char		*str;
	int		num;
	char		*endPtr;

	ASSERTARGS (dest && filename);

    errno = 0;
    fPtr = fopen(filename, "r");
    if ( ! fPtr ){

    	fprintf (stderr, "%s: Error could not open start_id file! <%s>\n",
                progName, filename);
        printf("System error message: %s\n\n", strerror(errno));
        return ztOpenFileError;
    }

    while (fgets(line, 1024, fPtr)){

    	count++;

    	if (count > 1) // one lonely line
    		break;

    }

    fclose(fPtr);

    if (count > 1){

    	return ztMissFormatFile;
    }

    if (line[strlen(line) -1] == '\n') { /* remove linefeed */

    	line[strlen(line) -1] = '\0';

    }

    str = strdup (line); /* this can fail - malloc */
    removeSpaces(&str);

	if (strspn(str, "1234567890") != strlen(str) ||
		strlen(str) != 3 ){

		fprintf (stderr, "%s: Error; file <%s> has invalid start_id : <%s>\n",
				progName, filename, line);
		return ztMissFormatFile;
	}

	num = (int) strtol (str, &endPtr, 10);
	if ( *endPtr != '\0'){
		fprintf (stderr, "%s: Error; file <%s> has invalid start_id : <%s>\n",
				progName, filename, line);
		return ztMissFormatFile;
	}

	//num++; // wrong! - we need last downloaded number

	if (num > 1000){ /* this needs a solution */
		fprintf (stderr, "%s: Error; start_id is 1000. Better figure out what to do!!!\n", progName);
		return ztUnknownError;
	}

	sprintf (str, "%d", num);

	*dest = str;

	return ztSuccess;
}

int writeStart_Id (char *idStr, char *filename){

	FILE		*fPtr;
	int 		result;

	ASSERTARGS (idStr && filename);

	if (strspn(idStr, "1234567890") != strlen(idStr) ||
		strlen(idStr) != 3 ){

		fprintf (stderr, "%s: Error invalid start_id argument: <%s>\n",
				progName, idStr);
		return ztInvalidArg;
	}

    errno = 0;
    fPtr = fopen(filename, "w");
    if ( ! fPtr ){

    	fprintf (stderr, "%s: Error could not create start_id file! <%s>\n",
                progName, filename);
        printf("System error message: %s\n\n", strerror(errno));
        return ztOpenFileError;
    }

    result = fprintf (fPtr, "%s\n", idStr);
    if (result != 4) { // THIS IS AN ERROR
    	fprintf(stdout, "Warnnnnning: writeStart_Id () : fprintf() did not write 4 bytes!\n");
    }

	fclose(fPtr);
	fflush (fPtr);


	return ztSuccess;
}

static DL_ELEM* findElemSubstring (DL_LIST *list, char *subStr){

	DL_ELEM	*elem = NULL;

	DL_ELEM	*mvrElem;
	char			*elemStr;
	int			found = 0;

	ASSERTARGS (list && subStr);

	/* sub string is made of 3 digits, I ignore case handling */

	if (DL_SIZE(list) == 0)

		return elem;


	mvrElem = DL_HEAD(list);
	while (mvrElem){

		elemStr = (char *) DL_DATA(mvrElem);
		if (strstr(elemStr, subStr)){

			found = 1;
			break;
		}


		mvrElem = DL_NEXT(mvrElem);
	}

	if (found)

		elem = mvrElem;

	return elem;
}

int logMessage (FILE *to, char *txt){

	char		*timestamp = NULL;
	pid_t		myPID;

	ASSERTARGS (to && txt);

	timestamp = formatMsgHeadTime();
	myPID = getpid();

	fprintf (to, "%s [%d] %s\n", timestamp, (int) myPID, txt);

	return ztSuccess;
}

/* opens toFile for appending, fromList is assumed sorted, appends strings
 * in element starting from startElem to end of list - update script should
 * empty or delete this file when done updating successfully */
int writeNewerFile (char const *toFile, DL_ELEM *startElem, DL_LIST *fromList){

	FILE    *filePtr = NULL;
	char   *str;
	DL_ELEM   *elem;

	ASSERTARGS(toFile && startElem && fromList);

	if (DL_SIZE(fromList) < 2){

		fprintf (stderr, "%s: Error newer list parameter size < 2. Files should be in pairs!\n",
				progName);
		return ztInvalidArg;
	}

	errno = 0;
	filePtr = fopen(toFile, "a"); /* open file for appending mode */
	if ( ! filePtr ){

		fprintf (stderr, "%s: Error could not create newer file! <%s>\n",
				      progName, toFile);
		printf("System error message: %s\n\n", strerror(errno));
		return ztOpenFileError;
	}

	elem = startElem;
	while (elem){

		assert (DL_DATA(elem));

		str = (char *) DL_DATA(elem);

		fprintf(filePtr, "%s\n", str);

		elem = DL_NEXT(elem);
	}

	fclose(filePtr);
	fflush (filePtr);

	return ztSuccess;

}
