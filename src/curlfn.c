/* curlfn.c:
 *
 *  Created on: March 31, 2021
 *  Author: Wael Hammoudeh
 *
 * This file contains two practical functions that leverage the libcurl library.
 * It aims to consolidate commonly used libcurl functionalities, making them
 * readily accessible for various applications.
 *
 * Minimum required Curl Library version is "7.80".
 *
 * The main functions are:
 *
 *  1 - download2File() --> download2FileRetry()
 *  2 - performQuery()  --> performQueryRetry()
 *
 * NOTES:
 *
 *  - Utilizes curl URL API for URL parsing by libcurl.
 *    Refer to 'man curl_url' for details.
 *
 *  - Users acquire two handles (pointers):
 *    CURLU* (URL parse handle) and CURL* (easy handle).
 *    The former is obtained with initialURL(), and the latter
 *    with initialOperation().
 *
 *  - Both handles are connected in initialOperation() function.
 *
 *  - Note: Function initialOperation() has replaced both
 *    initialDownload() and initialQuery() functions.
 *
 *  - Using the CURLU parse handle, libcurl enables users to modify/set
 *    the URL whole or in parts.
 *
 * Typical Usage: see Usage in download2File() & performQuery() below please.
 *
 *  - call functions in this order:
 *
 *    *) initialCurlSession()
 *    *) initialURL(url_prefix) : url_prefix include (scheme + server_name + path);
 *                                path can be partial & not empty.
 *                                Return parseHandle.
 *    *) initialOperation(parseHandle, securityToken) : parseHandle from above,
 *                                                      securityToken can be NULL
 *                                                      Return cHandle
 *    *) adjust path part in parseHandle before download2FileRetry()
 *       OR
 *       set / replace query part in parseHandle before performQueryRetry()
 *    *) download2FileRetry() or  performQueryRetry()
 *    *) Repeat (loop) above 2 steps as many times as needed
 *    *) easyCleanup(cHandle)
 *    *) urlCleanup(parseHandle)
 *    *) closeCurlSession()
 *
 *******************************************************************/

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>
#include <unistd.h>

#ifndef ZTERROR_H_
#include "ztError.h"
#endif

#ifndef UTIL_H_
#include "util.h"
#endif

#include "curlfn.h"

/* global exported variables:
 *
 *  - FILE *curlRawDataFP;
 *    client may set this to an open file pointer, when set;
 *    performQuery() function will write raw query response
 *    (as received) to that open file.
 *
 *  - FILE *curlLogtoFP;
 *    client may set this to an open file pointer, when set,
 *    selected - failure - messages are written to open file.
 *
 *    Each logged message starts with this line:
 *
 *     "- - - - - - - - - - - - Start Curl Functions Log - - - - - - - - - - - - - - -"
 *
 *	  And each logged message ends with this line:
 *
 *	   "- - - - - - - - - - - - End Curl Functions Log - - - - - - - - - - - - - - - -"
 *
 *	  See writeLogCurl() function.
 *
 ******************************************************************************/
FILE *curlRawDataFP = NULL;
FILE *curlLogtoFP = NULL;

/* global READ only variables:
 *
 *  - long sizeDownload;
 *
 *  - long responseCode;
 *
 *  - char curlErrorMsg[CURL_ERROR_SIZE + 1];
 *    buffer usually has error message strings from curl library.
 *    we fill buffer with our own string in few cases (bad me).
 *    User may retrieve a human readable explanation for error.
 *    Set with option CURLOPT_ERRORBUFFER - in initialQuery()
 *    & initialDownload().
 *
 ****************************************************************************/

long  sizeDownload = 0L;
long  responseCode = 0L;

char  curlErrorMsg[CURL_ERROR_SIZE + 1] = {0};

/* WriteMemoryCallback() function is only available in this file **/
static size_t WriteMemoryCallback (void *contents, size_t size, size_t nmemb, void *userp);

/* private global variables */
static int   sessionFlag = 0; /* initial flag - private */

/* initialCurlSession():
 *
 * Function initializes the libcurl session, verifying the libcurl version
 * and invoking curl_global_init() to set up the environment. It must be called
 * at the beginning of the session before utilizing other functions in this module.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * ztSuccess on successful initialization.
 * ztOldCurl if libcurl version is less than 7.86
 * CURLcode error code if curl_global_init() fails.
 *
 *******************************************************************************/

int initialCurlSession(void){

  CURLcode	result;
  curl_version_info_data *verInfo;

  if (sessionFlag) /* call me only once */

    return ztSuccess;

  verInfo = curl_version_info(CURLVERSION_NOW);

  if (verInfo->version_num < MIN_CURL_VER){

    fprintf (stderr, "ERROR: Old \"libcurl\" version found. Minimum version required is: %u.%u.%u\n"
	     "Please upgrade your \"libcurl\" installation.\n", MAJOR_REQ, MINOR_REQ, PATCH_REQ);
    return ztOldCurl;
  }

  result = curl_global_init(CURL_GLOBAL_ALL);
  if (result != 0){
    fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(result));

    return result;

  }

  sessionFlag = 1;

  return ztSuccess;
}

/* closeCurlSession(): ends the session, call when done.
 * ****************************************************************/

/* closeCurlSession():
 *
 * Function finalizes the libcurl session, performing necessary cleanup and
 * releasing resources. It should be called at session end.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * Void.
 *
 *****************************************************************************/

void closeCurlSession(void){

  if (sessionFlag == 0)

    return;

  /* client is responsible to cleanup CURL easy handle
   * and CURLU parse handle when done. **/

  /* we're done with libcurl, so clean it up **/
  curl_global_cleanup();

  sessionFlag = 0;

  return;
}

/* initialURL(): initials curl URL parser.
 * parameter 'server' must be null terminated string and pass isGoodURL()
 * function test.
 *
 * Usually this has (scheme + server_name + path) -- note no empty strings.
 * The path part might be "partial path", this is adjusted and replaced before
 * each download call.
 *
 * Return: CURLU pointer or NULL on error.
 * Important Note: caller must call urlCleanup() when done.
 *
 ****************************************************************************/

CURLU * initialURL (const char *server){

  CURLU       *retHandle = NULL;
  CURLUcode   curlCode;

  ASSERTARGS(server);

  if (sessionFlag == 0){
    fprintf(stderr, "initialURL(): Error, curl session not initialized.\n "
	    " initialCurlSession() first and check its return value.\n");
    return NULL;
  }

  if (isGoodURL(server) != TRUE){
    fprintf(stderr, "initialURL(): Error failed isGoodURL() for server: <%s>\n"
    		" Might has disallowed character or has less than 3 parts;\n"
    		" server must include (scheme + server_name + path)\n", server);
    return retHandle;
  }

  retHandle = curl_url();

  if ( ! retHandle){

    fprintf(stderr, "initialURL(): FATAL ERROR failed curl_url() function.\n");

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "curlfn.c: Failed initialURL()\n"
    		  "initialURL(): FATAL ERROR failed curl_url() function.\n");

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return retHandle;
  }

  /* set parts of server we have **/
  curlCode = curl_url_set(retHandle, CURLUPART_URL, server, 0);
  if (curlCode != CURLUE_OK){

    fprintf(stderr, "initialURL(): Error failed curl_url_set() function.\n"
	    "Curl error message: {%s}.\n", curl_url_strerror(curlCode));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "curlfn.c: Failed initialURL()\n"
    		  "initialURL(): ERROR failed curl_url_set() function for option CURLUPART_URL\n"
    		  " with 'server' parameter: <%s>\n"
    		  " Curl error string message: {%s}.\n", server, curl_url_strerror(curlCode));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    curl_url_cleanup(retHandle);

    retHandle = NULL;
  }

  return retHandle;

} /* END initialURL() */

/* initialOperation():
 *
 *  Acquire CURL easy handle and sets common options using curl_easy_setopt(),
 *  effectively initialing download or query operation.
 *
 * Parameters:
 * srcUrl: A CURLU pointer (parse handle) obtained from initialURL()
 * representing the source URL.
 * secToken: A pointer to a secure token for authentication during download.
 *           Can be NULL for non-secure downloads.
 *
 * Returns:
 * A CURL* handle on successful initialization or NULL on failure.
 *
 * Note: This function is a replacement for both - now removed -
 * initialDownload() and initialQuery() functions.
 *
 ************************************************************************/

CURL *initialOperation (CURLU *srcUrl, char *secToken){

  CURLcode     result;
  static CURL  *opHandle = NULL;

  /* allow NULL for secToken **/
  ASSERTARGS (srcUrl);

  if (sessionFlag == 0){
    fprintf(stderr, "initialOperation(): Error, curl session not initialized. You must call\n "
	    " initialCurlSession() first and check its return value.\n");
    return opHandle;
  }

  opHandle = easyInitial(); /* from curlfn.h: #define easyInitial() curl_easy_init() **/
  if ( ! opHandle){

    /* curl library does not know our error buffer yet, so write this in it **/
    sprintf(curlErrorMsg, "Failed curl_easy_init(): FATAL no 'opHandle' in initialOperation()\n");
    fprintf(stderr, "initialOperation(): FATAL ERROR curl_easy_init() call failed. Client:: Abort?!\n");

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "initialOperation(): FATAL ERROR curl_easy_init() "
    		  "call failed. Client:: Abort?!\n");

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return opHandle;
  }

  /* set common options **/

  /* tell curl to write human error messages in this buffer "curlErrorMsg" **/
  result = curl_easy_setopt (opHandle, CURLOPT_ERRORBUFFER, curlErrorMsg);
  if (result != CURLE_OK) {

    sprintf(curlErrorMsg, "Failed curl_easy_setopt() parameter: CURLOPT_ERRORBUFFER");
    fprintf(stderr, "initialOperation() Error: failed to set CURLOPT_ERRORBUFFER option.\n"
	    " Curl string error: %s\n", curl_easy_strerror(result));

    easyCleanup(opHandle);
    opHandle = NULL;
    return opHandle;
  }

  /* option was set successfully, now we trust curl library
   * will write error messages there.
   ********************************************************/

  /* no progress meter please **/
  result = curl_easy_setopt(opHandle, CURLOPT_NOPROGRESS, 1L);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_NOPROGRESS.\n");

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  /* request a larger receive buffer from libcurl **/
  result = curl_easy_setopt(opHandle, CURLOPT_BUFFERSIZE, 102400L);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_BUFFERSIZE.\n");

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  /* this call ties (connects) CURLU * (curl parse handle) with
   * CURL* (curl easy handle).
   * obtain curl parse handle with initialURL() function. **/
  result = curl_easy_setopt(opHandle, CURLOPT_CURLU, srcUrl);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_CURLU.\n");

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  result = curl_easy_setopt(opHandle, CURLOPT_USERAGENT, CURL_USER_AGENT);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_USERAGENT.\n");

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  result = curl_easy_setopt(opHandle, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_HTTP_VERSION.\n");

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  /* secToken might be NULL **/
  if (secToken){
    result = curl_easy_setopt(opHandle, CURLOPT_COOKIE, secToken);
    if(result != CURLE_OK) {

      fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_COOKIE.\n");

      easyCleanup(opHandle);
      opHandle = NULL;

      return opHandle;
    }
  }

  result = curl_easy_setopt(opHandle, CURLOPT_TCP_KEEPALIVE, 1L);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_TCP_KEEPALIVE.\n");

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  /* curl library default is no compression support;
   * setting last parameter to empty string ("") in
   * curl_easy_setopt() enables all supported built-in
   * compressions.
   *
   * NOTE: does not seem to help download speed. tested by
   * downloading same ten change files with and without this
   * option.
   *
   * maybe because .osc (change files) are already
   * compressed (gzipped) .gz at geofabrik.de server?
   *
   * The "index.html" page file failed size test in my
   * downlod2File() function, was compressed by server.
   *
   * This option may help when downloading large text files
   * only. Commented out and could be used in future.
   *
   ************************************************************/

  /** commented out -- does not help download speed in "getdiff"

  result = curl_easy_setopt(opHandle, CURLOPT_ACCEPT_ENCODING, "");
  if(result != CURLE_OK) {

    fprintf(stderr, "initialOperation(): Error failed curl_easy_setopt() : CURLOPT_ACCEPT_ENCODING.\n");

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }
  *******************************************************************/

  result = curl_easy_setopt (opHandle, CURLOPT_HTTPGET, 1L);
  if(result != CURLE_OK) {
    fprintf(stderr, "initialOperation() failed curl_easy_setopt for CURLOPT_HTTPGET\n"
	    " Curl string error: %s\n", curl_easy_strerror(result));

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  /* I use default with no timeout; this is here if you want to change it **/
  result = curl_easy_setopt (opHandle, CURLOPT_TIMEOUT, 0L);
  if(result != CURLE_OK) {
    fprintf(stderr, "initialOperation() failed curl_easy_setop CURLOPT_TIMEOUT\n"
	    " Curl string error: %s\n", curl_easy_strerror(result));

    easyCleanup(opHandle);
    opHandle = NULL;

    return opHandle;
  }

  return opHandle;

} /* END initialOperation() */

/* isConnCurl(): is connected curl?
 *
 * Checks network connection to the specified server parameter using the CURL
 * library API. This function does not require an active libcurl session.
 *
 * Parameters:
 * server: A null-terminated string representing the server to test
 *         the network connection to.
 *
 * Returns:
 * ztSuccess for a successful network connection.
 * ztNoConnNet failed to establish a network connection.
 *
 **************************************************************************/

int isConnCurl(const char *server){

  CURL  *cHandle;
  CURLcode  result;

  ASSERTARGS(server);

  cHandle = curl_easy_init();
  if( ! cHandle){

    fprintf(stderr, "isConnCurl(): Error failed curl_easy_handle() function.\n");
    sprintf(curlErrorMsg, "isConnCurl(): Error failed curl_easy_init(): Something went wrong");

    return FALSE;
  }

  result = curl_easy_setopt(cHandle, CURLOPT_URL, server);
  if(result != CURLE_OK){

    sprintf(curlErrorMsg, "isConnCurl(): Error failed setopt() CURLOPT_URL: %s",
	    curl_easy_strerror(result));
    fprintf(stderr, "isConnCurl(): Error failed to set URL option.\n");
    curl_easy_cleanup(cHandle);
    return FALSE;
  }

  result = curl_easy_setopt(cHandle, CURLOPT_CONNECT_ONLY, 1L);
  if(result != CURLE_OK){

    sprintf(curlErrorMsg, "isConnCurl(): Error failed setopt() CURLOPT_CONNECT_ONLY: %s",
	    curl_easy_strerror(result));
    fprintf(stderr, "isConnCurl(): Error failed to set CONNECT ONLY option.\n");
    curl_easy_cleanup(cHandle);
    return FALSE;
  }

  result = curl_easy_perform(cHandle);

  curl_easy_cleanup(cHandle);

  if(result == CURLE_OK)

    return ztSuccess;


  return ztNoConnNet;

} /* END isConnCurl() **/

/* download2File():
 * function downloads file from remote server to local machine.
 * remote file URL is setup in parameter CURLU* parseHandle, and
 * filename parameter is file path and name on local machine.
 *
 * Parameters:
 *  - filename: character pointer to local file; path included.
 *  - handle  : CURL easy handle; returned from initialOperation()
 *  - parseHandle: CURLU URL parse handle, returned from initialURL()
 *                 using URL for remote server; this must including:
 *                 scheme + server_name + path
 *
 * returns:
 *  - ztSuccess
 *  - ztFatalError: bad curl parse handle / pointer
 *  - result from isGoodFilename() on failure.
 *  - result from openOutputFile() on failure.
 *  - result from closeFile() on failure.
 *  - ztFailedLibCall if any curl library function fails.
 *  - ztFailedDownload
 *  - ztBadSizeDownload
 *  - ztResponse{code} - {response code including unknown & unhandled response codes}
 *
 * Usage:
 *  - initialCurlSession()
 *  - initialURL(server) : obtain 'parseHandle' parameter;
 *    'server' has (scheme + server_name + path); path maybe partial
 *  - initialOperation() : obtain 'handle' parameter
 *  - set 'path' part in parseHandle to the path of your file on remote server:
 *    curl_url_set (parseHandle, CURLUPART_PATH, newPath, 0); // newPath is remote path
 *  - download2File() : provide parameters described above.
 *  - cleanup & close curl session.
 *
 ****************************************************************************/

int download2File (char *filename, CURL *handle, CURLU *parseHandle){

  CURLcode   result, performResult; /* normal library curl return code **/
  int        myResult;
  FILE       *toFilePtr = NULL;

  long       sizeDisk = 0L; /* on disk file size, after download has completed **/
  long       sizeHeader = 0L;

  curl_off_t clSize; /* size at header content-length field : curl_off_t type **/

  curl_off_t dlSize;  /* download size as curl_off_t type **/


  ASSERTARGS (filename && handle && parseHandle);

  if (sessionFlag == 0){
    fprintf(stderr, "download2File(): Error, curl session not initialized. You must call\n "
	    " initialCurlSession() first and check its return value.\n");
    return ztNoCurlSession;
  }

  myResult = isGoodFilename(filename);
  if(myResult != ztSuccess){
    fprintf(stderr, "download2File(): Error parameter 'filename' is not good filename.\n");
    return myResult;
  }

  /* open destination file **/
  toFilePtr = openOutputFile(filename);
  if(!toFilePtr){
    fprintf(stderr, "download2File(): Error failed openOutputFile() "
    		"function; parameter 'filename': <%s>\n", filename);

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "download2File(): Error failed openOutputFile() "
      		"function; parameter 'filename': <%s>\n", filename);

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztOpenFileError;
  }

  /* tell curl library where to write data **/
  result = curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) toFilePtr);
  if (result != CURLE_OK){

    fprintf(stderr, "download2File(): Error failed curl_easy_setopt(); Parameter: CURLOPT_WRITEDATA.\n");

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "download2File(): Error failed curl_easy_setopt(); "
    		  "Parameter: CURLOPT_WRITEDATA.\n");

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztFailedLibCall;
  }

  /* set global sizeDownload to zero before perform() call **/
  sizeDownload = 0L;

  performResult = curl_easy_perform(handle);

  /* get some information, we return when curl getinfo() fails **/
  result = curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &responseCode);
  if (result != CURLE_OK){

    responseCode = -1;

    fprintf(stderr, "download2File(): Error failed curl_easy_getinfo() for CURLINFO_RESPONSE_CODE.\n");

    return ztFailedLibCall;
  }

  /* get sizeDownload **/
  result = curl_easy_getinfo(handle, CURLINFO_SIZE_DOWNLOAD_T, &dlSize);
  if(result != CURLE_OK){

    fprintf(stderr, "download2File(): Error failed curl_easy_getinfo() for "
	    "CURLINFO_SIZE_DOWNLOAD_T.\n");

    return ztFailedLibCall;
  }
  else {

    sizeDownload = (long) dlSize;
  }

  result = curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &clSize);
  if(result != CURLE_OK){

    fprintf(stderr, "download2File(): Error failed curl_easy_getinfo() for"
	    " CURLINFO_LENGTH_DOWNLOAD_T.\n");

    return ztFailedLibCall;
  }
  else {

    sizeHeader = (long) clSize;
  }

  /* check curl_easy_perform() return in our 'performResult' variable **/
  if (performResult == CURLE_COULDNT_CONNECT){

	/* connection was verified at initial, lost connection since **/
	fprintf(stderr, "download2File(): Failed curl_easy_perform() with curl could not connect error.\n");

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "download2File(): Error failed curl_easy_perform() function.\n"
    	  " function failed with (performResult == CURLE_COULDNT_CONNECT)\n"
	      " Parameter 'filename': <%s>\n"
	      " Remote URL: <%s>\n"
	      " curlErrorMsg contents: <%s>\n",
	      filename, getPrefixCURLU(parseHandle), curlErrorMsg);

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztNetConnFailed;

  }

  if (performResult == CURLE_COULDNT_RESOLVE_HOST){

	fprintf(stderr, "download2File(): Failed curl_easy_perform() with CURLE_COULDNT_RESOLVE_HOST.\n");

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "download2File(): Error failed curl_easy_perform() function.\n"
    	  " function failed with (performResult == CURLE_COULDNT_RESOLVE_HOST)\n"
	      " Parameter 'filename': <%s>\n"
	      " Remote URL: <%s>\n"
	      " curlErrorMsg contents: <%s>\n",
	      filename, getPrefixCURLU(parseHandle), curlErrorMsg);

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztHostResolveFailed;

  }

  if(performResult != CURLE_OK){

    fprintf(stderr,"download2File(): Error failed curl_easy_perform() function.\n");
    fprintf(stderr," Parameter 'filename': <%s>\n", filename);
    fprintf(stderr," Current Remote URL: <%s>\n", getPrefixCURLU(parseHandle));
    fprintf(stderr, " curl_easy_strerror() for result: <%s>\n", curl_easy_strerror(performResult));
    if(strlen(curlErrorMsg))
      fprintf(stderr, " curlErrorMsg contents: <%s>\n", curlErrorMsg);

    fprintf(stderr,"download2File(): Failed download with response code: <%ld>\n"
    		"Note (-1) value is for curl_easy_getinfo() failed to retrieve response code.", responseCode);
    /* if responseCode == -1L; we will not get to here. Note above is pointless! **/

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "download2File(): Error failed curl_easy_perform() function.\n"
	      " Parameter 'filename': <%s>\n"
	      " Remote URL: <%s>\n"
	      " curl_easy_strerror() for result: <%s>\n"
	      " curlErrorMsg contents: <%s>\n",
	      filename, getPrefixCURLU(parseHandle), curl_easy_strerror(result), curlErrorMsg);

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    if(toFilePtr){

      myResult = closeFile(toFilePtr);
      if(myResult != ztSuccess)
	    fprintf(stderr, "download2File(): Error failed closeFile() function.\n");

      /* we want to return ztFailedDownload; still 2 errors might be related?! **/
    }

    return responseCode2ztCode(responseCode);

  } /* end if(performResult != CURLE_OK) **/


  /* now performResult == CURLE_OK - all good from curl;
   * return error when we fail to close it **/
  myResult = closeFile(toFilePtr);
  if(myResult != ztSuccess){
    fprintf(stderr, "download2File(): Error failed closeFile() function.\n");
    return myResult;
  }

  /* get on disk file size, return on failure **/
  myResult = getFileSize(&sizeDisk, filename);
  if(myResult != ztSuccess){
    fprintf(stderr, "download2File(): Error failed getFileSize() function.\n");
    return myResult;
  }

  /* do not use size tests for index.html page; with option
   * CURLOPT_ACCEPT_ENCODING set, download2File() - getting the page - fails
   * this test.
   * my thinking is remote server compresses the page before sending, the on
   * disk file size is not equal to curl download size, I could be wrong.
   *
   * removed CURLOPT_ACCEPT_ENCODING setting option
   * this is wrong, need to use file type 'text' not name?
   *
   ******************************************************************/

  /*
  if((strcmp(lastOfPath(filename), "index.html") == 0)){

	return responseCode2ztCode(responseCode);

  }
  *******************************************************/

  if((performResult == CURLE_OK) && (responseCode == OK_RESPONSE_CODE)){

    if((sizeDisk == sizeHeader) || (sizeDisk == sizeDownload)){

      /* ALL three numbers should match;
       * but for now download is okay when fileSize matches at least
       * one of the other two numbers.
       *
       * Another thing to consider is to look inside and verify each
       * file:
       * state.txt file must have 2 lines timestamp & sequenceNumber
       * osc change files -might be compressed- is text XML file
       * with first line has:
       * <?xml version='1.0' encoding='UTF-8'?>
       * <osmChange version="0.6" generator="osmium/1.7.1">
       *
       **************************************************************/

      if( (sizeHeader != sizeDownload) && (curlLogtoFP) ){

	char   logBuffer[PATH_MAX] = {0};

	sprintf(logBuffer, "download2File(): Different sizes for sizeHeader and sizeDownload.\n"
		" sizeHeader is: <%ld> and sizeDownload is: <%ld>\n"
		" File disk size is: <%ld>\n"
		" Parameter 'filename': <%s>\n"
		" Current Remote URL: <%s>\n"
		" Note that non-named page does not have the header size set.\n",
		sizeHeader, sizeDownload, sizeDisk, filename, getPrefixCURLU(parseHandle));

	writeLogCurl(curlLogtoFP, logBuffer);
      }

      return ztSuccess;
    }
    else { /* failed fileSize test:  **/

      if(curlLogtoFP){

	char   logBuffer[PATH_MAX] = {0};

	sprintf(logBuffer, "download2File(): Warning, different sizes!\n"
		"download2File(): SUCCESS curl_easy_perform() function result == CURLE_OK.\n"
		" Response Code == 200 (OK_RESPONSE_CODE)\n"
		" Parameter filename: <%s>\n"
		" Current Remote URL: <%s>\n"
		" No function errors to include. But different sizes ...in bytes:\n"
		" sizeHeader is: <%ld> and sizeDownload is: <%ld>\n"
		" File disk size is: <%ld>\n",
		filename, getPrefixCURLU(parseHandle), sizeHeader, sizeDownload, sizeDisk);

	writeLogCurl(curlLogtoFP, logBuffer);
      }

      return ztBadSizeDownload;
    }

  } /* end if((result == CURLE_OK) && (responseCode == OK_RESPONSE_CODE)) **/

  return responseCode2ztCode(responseCode);

} /* END download2File() **/

/* Note change to download2File() function above.
 *
 * download2FileRetry():
 * calls download2File(),
 * retries failed results:
 *
 ********************************************************************************/

int download2FileRetry(char *destFile, CURL *handle, CURLU *parseHandle){

  ASSERTARGS(destFile && handle);

  if (sessionFlag == 0){
    fprintf(stderr, "download2FileRetry(): Error, curl session not initialized. You must call\n "
	    " initialCurlSession() first and check its return value.\n");
    return ztNoCurlSession;
  }

  int   result;
  int   delay = 2 * 30;  /* sleep time in seconds **/

  result = download2File(destFile, handle, parseHandle);

  if(result == ztSuccess)

    return result;

  if( (result == ztResponse500) ||
      (result == ztResponse502) ||
      (result == ztResponse503) ||
      (result == ztResponse504) ||
      (result == ztNetConnFailed) ||
      (result == ztHostResolveFailed) )

    sleep(delay);

  /* try again **/
  result = download2File(destFile, handle, parseHandle);
  if(result != ztSuccess){

    fprintf(stderr, "download2FileRetry(): Error failed download2File() for second attempt.\n"
    		" Sleep Time Value: <%d> seconds.\n"
    		" function failed with Zone Tree Code: <%s>\n", delay, ztCode2ErrorStr(result));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "download2FileRetry(): Error failed download2File() for second attempt.\n"
    		" Sleep Time Value: <%d> seconds.\n"
      		" function failed with Zone Tree Exit Code: <%s>\n", delay, ztCode2ErrorStr(result));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

  }

  return result;

} /* END download2FileRetry() **/


/* getPrefixCURLU():
 * Returns character pointer to string in initialed parse handle.
 *
 * Function requires scheme, host and path to be set; no empty string
 * is allowed. Function is used to verify the integrity of the pointer
 * in parseHandle.
 *
 * Parameter:
 *  parseUrlHandle : initialed CURLU* parse handle.
 *
 * Return:
 *  character pointer to string including {scheme + host + path} on success,
 *  NULL if any part is not set; has string length zero.
 *
 * Note some servers setup may not include {path} part; this is not the case
 * with my "getdiff" program or overpass servers we query (both have path part).
 *
 **************************************************************************/

char *getPrefixCURLU(CURLU *parseUrlHandle){

  CURLUcode  result; /* curl parse URL API return code **/

  char   *url = NULL;
  char   *scheme = NULL;
  char   *host = NULL;
  char   *path = NULL;
  char   myUrl[PATH_MAX] = {0};

  ASSERTARGS(parseUrlHandle);

  result = curl_url_get(parseUrlHandle, CURLUPART_SCHEME, &scheme, 0);
  if (result != CURLUE_OK ) {
    fprintf(stderr, "getPrefixCURLU(): Error failed curl_url_get() for 'scheme' part.\n");
    return url;
  }

  result = curl_url_get(parseUrlHandle, CURLUPART_HOST, &host, 0);
  if (result != CURLUE_OK ) {
    fprintf(stderr, "getPrefixCURLU(): Error failed curl_url_get() for 'host' part.\n");

    if(scheme) 	curl_free(scheme);

    return url;
  }

  result = curl_url_get(parseUrlHandle, CURLUPART_PATH, &path, 0);
  if (result != CURLUE_OK ) {
    fprintf(stderr, "getPrefixCURLU(): Error failed curl_url_get() for 'path' part.\n");

    if(scheme) 	curl_free(scheme);
    if(host)  curl_free(host);

    return url;
  }

  /* no zero length string is allowed in any part.
   * NOTE: path might be zero length in some servers setup **/
  if ( (strlen(scheme) == 0) || (strlen(host) == 0) || (strlen(path) == 0) )

    return url;

  sprintf(myUrl, "%s://%s%s", scheme, host, path);

  url = STRDUP(myUrl); /* STRDUP() terminates program on memory failure! **/

  if(scheme)
    curl_free(scheme);

  if(host)
    curl_free(host);

  if(path)
    curl_free(path);

  return url;

} /* END getPrefixCURLU() **/

/* callback function prototype: (see man CURLOPT_WRITEFUNCTION)
 *  size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
 ***************************************************************************/

static size_t WriteMemoryCallback (void *contents, size_t size, size_t nmemb, void *userp) {

  size_t realsize = size * nmemb;
  MEMORY_STRUCT *mem = (MEMORY_STRUCT *) userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {

    fprintf(stderr, "WriteMemoryCallback(): Error not enough memory "
	    "(realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;

} /* END WriteMemoryCallback() */

/* performQuery():
 * Executes a query using cURL and manages the interaction with the
 * provided server handle.
 *
 * Parameters:
 * - dst: Pointer to MEMORY_STRUCT where query results will be stored.
 * - whichData: The query string to be processed.
 * - qHandle: cURL handle for performing the query.
 * - srvrHandle: cURL URL handle for server interactions.
 *
 * Returns:
 *  - ztSuccess: Query execution successful
 *  - ztNoCurlSession: The cURL session was not initialized
 *  - ztInvalidArg: Invalid argument passed to this function
 *  - ztFailedLibCall: failed curl library function
 *  - ztNetConnFailed: Connection failure while performing the query
 *  - ztCurlTimeout: curl operation timed out
 *  - ztFailedDownload: failed curl_easy_perform() function call
 *  - ztNotString: response contents does not end with null character
 *  - ztBadDownloadSize: download size did not match byte count in MEMORY_STRUCT
 *  - ztResponse400: bad request (query); query syntax error
 *  - ztResponse429: too many request in short time
 *  - ztResponse504: busy / overloaded overpass server
 *  - ztResponseNNN: Various response codes indicating specific issues
 *  - ztUnknownError: unknown and not handled error by this function
 *
 * Additional Notes:
 *  - Utilizes cURL functionality to perform the query.
 *  - Handles query-related settings, URL encoding, and error checks.
 *  - Provides detailed error messages for various failure scenarios.
 *  - Manages the retrieval of response codes and logging.
 *  - Writes query result data to a client file pointer if set (curlRawDataFP).
 *
 * Usage:
 *  - initialCurlSession()
 *  - initialURL(server) : obtain 'srvrHandle' parameter;
 *    your 'server' parameter here has (scheme + server_name + path)
 *  - initialOperation() : obtain 'qHandlel' parameter.
 *  - initialMS() to obtain parameter 'dst' MEMORY_STRUCT
 *  - whichData parameter is your constructed "query string".
 *  - cleanup & close curl session.
 *
 * TODO: set timer & signal; abort query?
 *       see "set_timer.notes" in paths2
 *
 ********************************************************************/

int performQuery (MEMORY_STRUCT *dst, char *whichData, CURL *qHandle, CURLU *srvrHandle) {

  CURLUcode  resultU; /* URL API curl parser calls return code **/

  CURLcode   resultC, performResult; /* normal curl library return code **/

  char       *queryEscaped;
  char       *serverName;
  char       getBuf[PATH_MAX] = {0};

  long       dlSize;


  ASSERTARGS (dst && whichData && qHandle && srvrHandle);

  if (sessionFlag == 0){
    fprintf(stderr, "performQuery(): Error, session not initialized. You must call\n "
	    " curlInitialSession() first and check its return value.\n");
    return ztNoCurlSession;
  }

  if(dst->size != 0){
    fprintf(stderr, "performQuery(): Error size member in 'dst' "
	    "parameter is not zero, use initialMS().\n");
    return ztInvalidArg;
  }

  if (strlen(whichData) == 0){
    fprintf(stderr, "performQuery(): Error empty (query string), whichData parameter!\n");
    return ztInvalidArg;
  }

  /* check that initialURL() was called, try to get serverName - overhead? NO. */
  resultU = curl_url_get (srvrHandle, CURLUPART_HOST, &serverName, 0);
  if(resultU != CURLUE_OK) {

    fprintf(stderr, "performQuery(): Error failed curl_url_get() call for HOST name. "
	    " Curl URL string error: %s\n", curl_url_strerror(resultU));

    return ztFailedLibCall;
  }

  if (serverName == NULL || strlen(serverName) == 0){
    fprintf(stderr, "performQuery(): Error server name NOT set in srvrHandle parameter!"
	    " did you call initialURL()?\n");
    return ztInvalidArg;
  }

  /* let curl do URL encoding for our query string **/
  queryEscaped = curl_easy_escape(qHandle, whichData, strlen(whichData));
  if (!queryEscaped){
    printf("performQuery(): Error failed curl_easy_esacpe() function.\n");
    return ztFailedLibCall;
  }

  /* prepend "data=" prefix to the encoded query string */
  sprintf(getBuf, "data=%s", queryEscaped);

  /* replace / set query part in curl parse handle srvrHandle */
  resultU = curl_url_set(srvrHandle, CURLUPART_QUERY, getBuf, 0);
  if(resultU != CURLUE_OK) {
    fprintf(stderr, "performQuery(): Error failed curl_url_set() call for query part. "
	    "Curl URL string error: %s\n", curl_url_strerror(resultU));
    return ztFailedLibCall;
  }

  /* set query specific easy options **/
  resultC = curl_easy_setopt(qHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  if(resultC != CURLE_OK) {
    fprintf(stderr, "performQuery(): Error failed curl_esay_setop CURLOPT_WRITEFUNCTION\n"
	    " Curl string error: %s\n", curl_easy_strerror(resultC));
    return ztFailedLibCall;
  }

  /* use user provided pointer to write to, this is passed to WriteMemoryCallback()
   * as fourth argument **/
  resultC = curl_easy_setopt(qHandle, CURLOPT_WRITEDATA, (void *)dst);
  if(resultC != CURLE_OK) {
    fprintf(stderr, "performQuery(): Error failed curl_easy_setopt() CURLOPT_WRITEDATA "
	    " Curl string error: %s\n", curl_easy_strerror(resultC));
    return ztFailedLibCall;
  }

  /* set sizeDownload to zero **/
  sizeDownload = 0L;

  /* uncomment for debug **/
  /* curl_easy_setopt(qHandle, CURLOPT_VERBOSE, 1L); */

  /* send it away! */
  performResult = curl_easy_perform(qHandle);

  /* get some information, we return when getinfo() fails **/
  resultC = curl_easy_getinfo (qHandle, CURLINFO_RESPONSE_CODE, &responseCode);
  if (resultC != CURLE_OK){

    responseCode = -1;

    fprintf(stderr, "performQuery(): Error failed curl_easy_getinfo() for CURLINFO_RESPONSE_CODE.\n");

    return ztFailedLibCall;
  }

  /* get sizeDownload **/
  resultC = curl_easy_getinfo(qHandle, CURLINFO_SIZE_DOWNLOAD_T, &dlSize);
  if(resultC != CURLE_OK){

    fprintf(stderr, "performQuery(): Error failed curl_easy_getinfo() for "
	    "CURLINFO_SIZE_DOWNLOAD_T.\n");

    return ztFailedLibCall;
  }
  else {

    sizeDownload = (long) dlSize;
  }

  if (queryEscaped)
    curl_free(queryEscaped);

  /* check performResult **/
  if(performResult != CURLE_OK){

    if(performResult == CURLE_OPERATION_TIMEDOUT){

      /* we get this error only if CURLOPT_TIMEOUT is set **/
      fprintf(stderr, "performQuery(): Error failed curl_easy_perform() for operation TIMED OUT.\n");
      return ztCurlTimeout;
    }
    else if (performResult == CURLE_COULDNT_CONNECT){

      fprintf(stderr, "performQuery(): Error failed curl_easy_perform() with curl could not connect.\n");
      return ztNetConnFailed;
    }
    else {

      /* failed with some other error code **/
      fprintf(stderr, "performQuery(): Error failed curl_easy_perform(); transfer failed:\n"
	      " curl string error: <%s>\n"
	      " curlErrorMsg contents: <%s>\n"
	      " HTTP Response Code: <%ld>\n\n",
	      curl_easy_strerror(performResult), curlErrorMsg, responseCode);
      //return responseCode2ztCode(responseCode);
      return ztFailedDownload;
    }

  }
  else { /* performResult == CURLE_OK **/

    if(responseCode == OK_RESPONSE_CODE){
      if((dst->memory[dst->size]) != '\0'){ /* last received character must be null **/
	fprintf(stderr, "performQuery(): Error response contents NOT string; not null ending.\n"
		" (performResult == CULRE_OK && responseCode == OK_RESPONSE_CODE)\n\n");
	return ztNotString;
      }
      else if(dst->size == 0 || strlen(dst->memory) == 0){
	fprintf(stderr, "performQuery(): Error empty query response. Nothing in MEMORY_STRUCT!\n");
	return ztEmptyString;
      }
      else if( (sizeDownload != dst->size) || (sizeDownload != strlen(dst->memory)) ){
	fprintf(stderr, "performQuery(): Error different sizes; sizeDownload test failed.\n"
		" (performResult == CULRE_OK && responseCode == OK_RESPONSE_CODE)\n\n");
	return ztBadSizeDownload;
      }
      else{ /* (performResult == CURLE_OK) &&
	       (responseCode == OK_RESPONSE_CODE) &&
	       passed our three tests above **/

	if(curlRawDataFP){
	  /* write response contents to open file pointer **/
	  fprintf(curlRawDataFP, "%s", dst->memory);
	  fflush(curlRawDataFP);
	}
	return responseCode2ztCode(responseCode);
      }
    }
    else { /* responseCode != OK_RESPONSE_CODE **/

      if(curlLogtoFP){

	char   logBuffer[PATH_MAX] = {0};

	sprintf(logBuffer, "performQuery(): Error failed test (responseCode == OK_RESPONSE_CODE)\n"
		" Received response code was: <%ld>\n", responseCode);
	if(strlen(dst->memory)){
	  sprintf(logBuffer + strlen(logBuffer), " String received is below:\n");

	  /* copy only what will fit in our logBuffer **/
	  strncpy(logBuffer + strlen(logBuffer), dst->memory, (size_t) (PATH_MAX - strlen(logBuffer)));
	}
	writeLogCurl(curlLogtoFP, logBuffer);
      }
      return responseCode2ztCode(responseCode);
    }
  }

  /* we should not get here! **/
  fprintf(stderr, "performQuery(): UNKNOWN ERROR,,, at bottom of function.\n"
	  "LAST curl error string is: <%s>\n\n", curl_easy_strerror(resultC));

  return ztUnknownError;

} /* END performQuery() */

/* performQueryRetry():
 *
 *  RETRY?
 *  - ztNotString;
 *  - ztBadSizeDownload;
 */

int performQueryRetry (MEMORY_STRUCT *dst, char *whichData, CURL *qHandle, CURLU *srvrHandle) {

  int  result;

  result = performQuery(dst, whichData, qHandle, srvrHandle);

  if ( (result == ztCurlTimeout) ||
       (result == ztNetConnFailed) ||
       (result == ztResponse504) ||
       (result == ztNotString) ||
       (result == ztBadSizeDownload)){

    sleep(SLEEP_SECONDS);

    fprintf(stdout, "performQueryRety(): Retrying query for error code: <%s>"
    		" RETRY RETRY\n\n", ztCode2ErrorStr(result));

    return performQuery(dst, whichData, qHandle, srvrHandle);
  }

  return result;

} /* END performQueryRetry() **/

/* isOkResponse(): remote server response is okay if first line in the
 * response matches header string supplied by client.
 * this function compares the first "header" string length characters from
 * "responseStr" to "header".
 * Question? : I am hoping this tells me the query was understood by
 * the server; the query syntax was correct.
 *
 * @@ THIS @@ This should return TRUE or FALSE. FIXME
 ********************************************************************/

int isOkResponse (char *responseStr, char *header){

  ASSERTARGS (responseStr && header);

  int retCode;

  if (strncmp (responseStr, header, strlen(header)) == 0){

    retCode = ztSuccess;
  }
  else {
    /**
    fprintf (stderr, "isOkResponse(): parameter 'header' did not match first part of response text.\n"
             " Parameter 'header' has: <%s>\n", header);
    fprintf (stderr, "isOkResponse(): Error: Not a valid response. Server may responded "
	     "with an error message! The server response was:\n\n");
    fprintf (stderr, " Start server response below >>>>:\n\n");
    fprintf (stderr, "%s\n\n", responseStr);
    fprintf (stderr, " >>>> End server response This line is NOT included.\n\n");
    **/

    retCode = ztStringUnknown;
  }

  return retCode;

} /* END isOkResponse() */

int writeLogCurl(FILE *to, char *msg){

  /* TODO: add HEADER and FOOTER to message here **/

  ASSERTARGS (to && msg);

  if(strlen(msg) == 0){
    fprintf(stderr,"writeLogCurl(): Error 'msg' parameter is empty.\n");
    return ztInvalidArg;
  }

  if(strlen(msg) > PATH_MAX){
    fprintf(stderr,"writeLogCurl(): Error 'msg' parameter is longer than PATH_MAX: <%d>.\n", PATH_MAX);
    return ztInvalidArg;
  }

  char   *timestamp = NULL;
  pid_t   myPID;

  timestamp = formatMsgHeadTime();
  if(! timestamp){
    fprintf(stderr,"writeLogCurl(): Error failed formatMsgHeadTime().\n");
    return ztMemoryAllocate;
  }

  myPID = getpid();

  fprintf(to, "\n- - - - - - - - - - - - Start Curl Functions Log - - - - - - - - - - - - - - -\n\n");

  fprintf (to, "%s [%d] writeLogCurl() received message below:\n %s\n", timestamp, (int) myPID, msg);

  fprintf(to, "- - - - - - - - - - - - End Curl Functions Log - - - - - - - - - - - - - - - -\n\n");

  return ztSuccess;

} /* END writeLogCurl() **/

/* responseCode2ztCode():
 * Converts HTTP response code to our Zone Tree code -- see ztError.h file.
 *
 *
 ***************************************************************************/

ZT_EXIT_CODE responseCode2ztCode(long resCode){

  /* special cases **/
  if(resCode == 200)

    return ztSuccess;

  else if (resCode == -1)

    /* if curl library fails to retrieve response code (curl_easy_getinfo()),
     * in that case we set global responseCode = -1L;
     * functions (download2File() & performQuery()) usually will exit with
     * "ztFailedLibCall" error code; you may not get this!
     *******************************************************************/
    return ztResponseFailed2Retrieve;

  else if (resCode == 0)

    return ztResponseNone;

  else ; // do nothing?

  /* handle response codes in a switch statement **/
  switch((int) resCode){

  case 301:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 301.\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse301));

    return ztResponse301;
    break;

  case 400:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 400.\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse400));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "Response Code 400 Response Code 400 Response Code 400\n"
	      "responseCode2ztCode(): Error received Server Response Code <400>\n Code is for: <%s>\n",
	      ztCode2Msg(ztResponse400));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse400;
    break;

  case 403:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 403\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse403));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "Response Code 403 Response Code 403 Response Code 403\n"
	      "responseCode2ztCode(): Error received Server Response Code <403>\n Code is for: <%s>\n",
	      ztCode2Msg(ztResponse403));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse403;
    break;


  case 404:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 404.\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse404));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "Response Code 404 Response Code 404 Response Code 404\n"
	      "responseCode2ztCode(): Error received Server Response Code <404>\n Code is for: <%s>\n",
	      ztCode2Msg(ztResponse404));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse404;
    break;

  case 429:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 429\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse429));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "Response Code 429 Response Code 429 Response Code 429\n"
	      "responseCode2ztCode(): Error received Server Response Code <429>\n Code is for: <%s>\n",
	      ztCode2Msg(ztResponse429));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse429;
    break;

  case 500:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 500.\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse500));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "Response Code 500 Response Code 500 Response Code 500\n"
	      "responseCode2ztCode(): Error received Server Response Code <500>\n Code is for: <%s>\n",
	      ztCode2Msg(ztResponse500));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse500;
    break;

  case 502:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 502.\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse502));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer,"responseCode2ztCode(): Error received Server Response Code <502>\n Code is for: <%s>\n",
	      ztCode2Msg(ztResponse502));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse502;
    break;

  case 503:

    fprintf(stderr, "responseCode2ztCode(): Error received Server Response Code 503.\n Code is for: <%s>\n",
	    ztCode2Msg(ztResponse503));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer, "Response Code 503 Response Code 503 Response Code 503\n"
	      "responseCode2ztCode(): Error received Server Response Code <503>\n Code is for: <%s>\n",
	      ztCode2Msg(ztResponse503));

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse503;
    break;


  default:

    fprintf(stderr, "responseCode2ztCode(): Error unhandled response code. THIS IS THE DEFAULT CASE.\n"
	    " RESPONSE CODE IS NOT HANDLED BY THIS FUNCTION. RESPONSE CODE NUMBER IS: <%ld>.\n", resCode);

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

      sprintf(logBuffer,"responseCode2ztCode(): Error unhandled response code. THIS IS THE DEFAULT CASE.\n"
	      " RESPONSE CODE IS NOT HANDLED BY THIS FUNCTION. CODE IS: <%ld>.\n", resCode);

      writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponseUnhandled;
    break;

  } /* end switch(curlResponseCode) **/

  fprintf(stderr, "responseCode2ztCode(): Error: THIS IS OUT SIDE OF SWITCH STATEMENT AND SHOULD NOT BE SEEN.\n");

  if(curlLogtoFP){

    char   logBuffer[PATH_MAX] = {0};

    sprintf(logBuffer, "responseCode2ztCode(): Error: THIS IS OUT SIDE OF SWITCH STATEMENT AND SHOULD NOT BE SEEN.\n");

    writeLogCurl(curlLogtoFP, logBuffer);
  }

  return ztUnknownError; /* we should not get here! **/

} /* END responseCode2ztCode() **/

/* initialMS():
 * Initializes a MEMORY_STRUCT object by allocating memory for the
 * structure itself and for the memory field. The size field is set
 * to zero.
 *
 * Parameter:
 * None.
 *
 * Return:
 * A pointer to one MEMORY_STRUCT on success, NULL on failure.
 *
 ***************************************************************/

MEMORY_STRUCT *initialMS(void){

  MEMORY_STRUCT *newMS;

  newMS = (MEMORY_STRUCT *) malloc(sizeof(MEMORY_STRUCT));
  if ( ! newMS ){
	fprintf(stderr, "initialMS(): Error allocating memory.\n");
	return newMS;
  }
  newMS->memory = (char *) malloc(1); /* IMPORTANT to allocate this */
  if(! newMS->memory){
    fprintf(stderr, "initialMS(): Error allocating memory.\n");
    free(newMS);
    newMS = NULL;
    return newMS;
  }

  newMS->size = 0;

  return newMS;

} /* END initialMS() **/

/* zapMS():
 * Releases memory allocated for a MEMORY_STRUCT object,
 * including the memory allocated for the memory field.
 *
 * Parameter:
 * Pointer to a pointer to MEMORY_STRUCT (MEMORY_STRUCT**).
 *
 * Return:
 * Nothing.
 *
 * Note: passed in parameter can NOT be used after this call.
 *
 ********************************************************/

void zapMS(MEMORY_STRUCT **ms){

  MEMORY_STRUCT *myMS;

  ASSERTARGS(ms);

  myMS = (MEMORY_STRUCT *) *ms;

  if(!myMS) return;

  if(myMS->memory) free(myMS->memory);

  memset(myMS, 0, sizeof(MEMORY_STRUCT));

  free(myMS);

  *ms = NULL; /* Set original pointer to NULL after freeing the memory **/

  return;

} /* END zapMS() **/

