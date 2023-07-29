/* curlfn.c :
 *
 * There are a lot of settings available in libcurl, the functions in this file
 * bundle curl library functions I usually use in very few functions.
 * This is work in progress and may change.
 * The two main functions are:
 *
 *  1 - download2File()
 *  2 - performQuery()
 *
 * NOTES:
 *  - I use curl URL API; to let libcurl do URL parsing for us. See 'man curl_url'.
 *  - I initial downloads and queries using CURLU *handle (URL parse handle).
 *  - user gets 2 handles (pointers); CURLU* (URL parse handle) and CURL* (easy handle).
 *    the first with initialURL() and the second with initialDownload() or
 *    initialQuery().
 *  - using CURLU handle, libcurl enables user to change / set URL whole or part.
 *
 *******************************************************************/

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>
#include <unistd.h>

#include "curlfn.h"
#include "util.h"
#include "ztError.h"


/* global and exported variable *rawDataFP,
 * client may set this to an open file pointer,
 * if set, performQuery() function will write
 * query response to that file
 *
 * curlLogtoFP: if set; selected - failure - messages are written to open file.
 *
 ******************************************************************************/
FILE *curlRawDataFP = NULL;
FILE *curlLogtoFP = NULL;

long   sizeDownload;

char  curlErrorMsg[CURL_ERROR_SIZE + 1];

/* curlErrorMsg buffer is filled ONLY ON FAILURE by curl library.
 *
 * set with option CURLOPT_ERRORBUFFER - in initialQuery() & initialDownload().
 **************************************************************************/

/* WriteMemoryCallback() function is only available in this file */
static size_t WriteMemoryCallback (void *contents, size_t size, size_t nmemb, void *userp);

/* global variables */
static int   sessionFlag = 0; /* initial flag - private */

static CURLU   *parseUrlHandle = NULL;

/* initialCurlSession(): calls curl_global_init() after checking version.
 * first function to call, needed only once in a session.
 * TODO: is libcurl installed?
 *****************************************************************/
int initialCurlSession(void){

  CURLcode	result;
  curl_version_info_data *verInfo;

  if (sessionFlag) /* call me only once */

    return ztSuccess;

  verInfo = curl_version_info(CURLVERSION_NOW);
  if (verInfo->version_num < MIN_CURL_VER){

    fprintf (stderr, "ERROR: Old \"libcurl\" version found. Minimum version required is: 7.80.0\n"
	     "Please upgrade your \"libcurl\" installation.\n");
    return ztOldCurl;
  }

  result = curl_global_init(CURL_GLOBAL_ALL);
  if (result != 0){
    fprintf(stderr, "curl_global_init() failed: %s\n",
	    curl_easy_strerror(result));
    return result;

  }

  sessionFlag = 1;

  return ztSuccess;
}

/* closeCurlSession(): ends the session, call when done.
 * ****************************************************************/
void closeCurlSession(void){

  if (sessionFlag == 0)

    return;

  /* cleanup curl stuff - REMOVE: curl_easy_cleanup() is done per handle 12/19/2021 */
  // curl_easy_cleanup(curl_handle);

  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();

  sessionFlag = 0;

  return;
}

/* initialURL(): initials curl URL parser.
 * parameter 'server' must be null terminated string and pass isGoodURL()
 * function test.
 *
 * Return: CURLU pointer or NULL on error.
 * Caller must call urlCleanup(retValue) when done.
 ****************************************************************************/
CURLU * initialURL (const char *server){

  CURLU   *retHandle = NULL;
  CURLUcode   curlCode;

  ASSERTARGS(server);

  if (sessionFlag == 0){
    fprintf(stderr, "initialURL(): Error, curl session not initialized.\n "
	    " initialCurlSession() first and check its return value.\n");
    return NULL;
  }

  if (isGoodURL(server) != TRUE){
    fprintf(stderr, "initialURL(): Error invalid URL in parameter 'server'\n");
    return retHandle;
  }

  retHandle = curl_url();

  if ( ! retHandle){
    fprintf(stderr, "initialURL(): Error failed curl_url() function.\n");

    sprintf(curlErrorMsg, "Failed curl_url(): Out of Memory"); /* only reason this fails **/
    return retHandle;
  }


  /* set parts of server we have **/
  curlCode = curl_url_set(retHandle, CURLUPART_URL, server, 0);
  if (curlCode != CURLUE_OK){

    fprintf(stderr, "initialURL(): Error failed curl_url_set() function.\n"
	    "Curl error message: {%s}.\n", curl_url_strerror(curlCode));

    sprintf(curlErrorMsg, "Failed curl_url_set() CURLUPART_URL: %s",
    		curl_url_strerror(curlCode));

    curl_url_cleanup(retHandle);
    retHandle = NULL;
  }

  parseUrlHandle = retHandle;

  return retHandle;

} /* END initialURL() */

/* isConnCurl(): is connected curl? function tests net connection to 'server'
 * parameter using CURL library API.
 *
 * Function DOES NOT require curl session.
 *
 * Note: we could check connections to several known servers, if one fails we
 * fail?
 *
 * Return ztSuccess for okay connection or ztNoConnNet for no connection.
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

} /* END curlConnected() **/

/* initialDownload() : initial download, if secToken is not null then we have
 * secure download! function connects CURLU and CURL handles.
 * acquire CURL easy handle and sets basic options on it for download.
 *
 * user should call easyCleanup(h) or curl_easy_cleanup(h)
 *
 * returns CURL * handle on success or NULL on failure.
 ************************************************************************/
CURL *initialDownload (CURLU *srcUrl, char *secToken){

  CURLcode   result;
  CURL       *dwnldHandle = NULL;

  /* allow NULL for secToken **/
  ASSERTARGS (srcUrl);

  if (sessionFlag == 0){
    fprintf(stderr, "initialDownload(): Error, curl session not initialized. You must call\n "
	    " initialCurlSession() first and check its return value.\n");
    return dwnldHandle;
  }

  dwnldHandle = easyInitial();
  if ( ! dwnldHandle){

    /* curl library does not know our error buffer yet, so write this in it **/
	sprintf(curlErrorMsg, "Failed curl_easy_init(): Something went wrong");
    fprintf(stderr, "initialDownload(): curl_easy_init() call failed. Client:: Abort?!\n");
    return dwnldHandle;
  }

  /* tell curl to write human error messages in this buffer "curlErrorMsg" **/
  result = curl_easy_setopt (dwnldHandle, CURLOPT_ERRORBUFFER, curlErrorMsg);
  if (result != CURLE_OK) {

	sprintf(curlErrorMsg, "Failed curl_easy_setopt() parameter: CURLOPT_ERRORBUFFER");
    fprintf(stderr, "initialDownload() Error: failed to set ERRORBUFFER option\n"
	    " curl_easy_setopt(.., CURLOPT_ERRORBUFFER, ..) failed: %s\n",
		curl_easy_strerror(result));
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  /* no progress meter please */
  result = curl_easy_setopt(dwnldHandle, CURLOPT_NOPROGRESS, 1L);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_NOPROGRESS.\n");
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  /* option was set successfully, now we trust curl library
   * to write error messages there.
   ********************************************************/

  result = curl_easy_setopt(dwnldHandle, CURLOPT_BUFFERSIZE, 102400L);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_BUFFERSIZE.\n");
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  /* caller initials CURLU (parse handle) with part or complete URL.
   * this call ties (connects) CURLU * (curl parse handle) with
   * CURL* (curl easy handle). */
  result = curl_easy_setopt(dwnldHandle, CURLOPT_CURLU, srcUrl);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_CURLU.\n");
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  result = curl_easy_setopt(dwnldHandle, CURLOPT_USERAGENT, CURL_USER_AGENT);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_USERAGENT.\n");
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  result = curl_easy_setopt(dwnldHandle, CURLOPT_MAXREDIRS, 50L);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_MAXREDIRS.\n");
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  result = curl_easy_setopt(dwnldHandle, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_HTTP_VERSION.\n");
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  /* secToken might be NULL **/
  if (secToken){
    result = curl_easy_setopt(dwnldHandle, CURLOPT_COOKIE, secToken);
    if(result != CURLE_OK) {

      fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_COOKIE.\n");
      easyCleanup(dwnldHandle);
      dwnldHandle = NULL;
      return dwnldHandle;
    }
  }

  result = curl_easy_setopt(dwnldHandle, CURLOPT_TCP_KEEPALIVE, 1L);
  if(result != CURLE_OK) {

    fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_TCP_KEEPALIVE.\n");
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  /* fail when response code >= 400; fail on error **/
  result = curl_easy_setopt(dwnldHandle, CURLOPT_FAILONERROR, 1L);
  if (result != CURLE_OK) {

    fprintf(stderr, "initialDownload() Error: failed to set CURLOPT_FAILONERROR option\n"
	    " curl_easy_setopt(.., CURLOPT_FAILONERROR, ..) failed: %s\n",
		curl_easy_strerror(result));
    easyCleanup(dwnldHandle);
    dwnldHandle = NULL;
    return dwnldHandle;
  }

  /* set exported sizeDownload to zero **/
  sizeDownload = 0;

  return dwnldHandle;

} /* END initialDownload() */

/* This is from curl/system.h header file regarding curl_off_t  ********************
  *
 * curl_off_t
 * ----------
 *
 * For any given platform/compiler curl_off_t must be typedef'ed to a 64-bit
 * wide signed integral data type. The width of this data type must remain
 * constant and independent of any possible large file support settings.
 *
 * END from curl/system.h file ****************************/

 /* given the above note, I believe it is safe to cast curl_off_t into (long)
  * on a 64-bit linux system
  *
  *  sizeHeader is cast on clSize, and sizeDownload is cast on dlSize:
  *
  *  sizeHeader = (long) clSize;
  *  sizeDownload = (long) dlSize;
  *
  *  another note: sizeDownload is an exported (global) variable - not local
  *  to this function; sizeDownload is set to zero in initialDownload().
  *
  *******************************************************************/

/* download2File():
 *    - toFilePtr : FILE pointer to an open file. user opens and closes file.
 *    - handle : pointer to an initial CURL handle
 *
 * returns:
 *  - result from isGoodFilename() on failure.
 *  - result from openOutputFile() on failure.
 *  - result from closeFile() on failure.
 *  - ztFailedLibCall if any curl library function fails.
 *  - ztFailedDownload
 *  - ztBadSizeDownload
 *  - ztResponse{code} - {unknown/unhandled}
 *	- ztSuccess:
 *
 * FILE* openOutputFile (char *filename)
 *
 * int closeFile(FILE *fPtr)
 *
 * Change: first parameter is character pointer to destination file - was
 *         FILE * to an open file.
 *
 * Function test first parameter for good filename only.
 * Function opens destination file using my openOutputFile() function.
 * Function opens file with writing mode; meaning its contents will be replaced
 * if it had any.
 * Function uses my closeFile() to close the file; it does some error checking!
 *
 ****************************************************************************/
int download2File (char *filename, CURL *handle){

  CURLcode   result;
  int        myResult;
  FILE       *toFilePtr = NULL;

  long       responseCode;

  ASSERTARGS (filename && handle);

  if (sessionFlag == 0){
    fprintf(stderr, "download2File(): Error, curl session not initialized. You must call\n "
	    " initialCurlSession() first and check its return value.\n");
    return ztNoSession;
  }

  myResult = isGoodFilename(filename);
  if(myResult != ztSuccess){
	fprintf(stderr, "download2File(): Error parameter 'filename' is not good filename.\n");
	return myResult;
  }

  /* open destination file **/
  toFilePtr = openOutputFile(filename);
  if(!toFilePtr){
	fprintf(stderr, "download2File(): Error failed openOutputFile() function.\n");
	return ztOpenFileError;
  }

  /* tell curl library where to write data **/
  result = curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) toFilePtr);
  if (result != CURLE_OK){

    fprintf(stderr, "download2File(): Error failed curl_easy_setopt(); Parameter: CURLOPT_WRITEDATA.\n");
    return ztFailedLibCall;
  }

  /* curl easy handle may get corrupted?! report / get error ASAP **/
  char   *currentUrl;

  currentUrl = getCurrentURL();
  if(!currentUrl)
	currentUrl = STRDUP("Failed getCurrentURL()");

  result = curl_easy_perform(handle);

  if(result != CURLE_OK){

	fprintf(stderr,"download2File(): Error failed curl_easy_perform() function.\n");
	fprintf(stderr," Parameter 'filename': <%s>\n", filename);
	fprintf(stderr," Current Remote URL: <%s>\n", currentUrl);
	fprintf(stderr, " curl_easy_strerror() for result: <%s>\n", curl_easy_strerror(result));
	if(strlen(curlErrorMsg))
	  fprintf(stderr, " curlErrorMsg contents: <%s>\n", curlErrorMsg);

	/* try to get Server Response Code **/
	if(curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &responseCode) != CURLE_OK)

	  responseCode = -1;

	fprintf(stderr,"download2File(): Failed download with response code: <%ld>\n", responseCode);

	if(curlLogtoFP){

	  char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "Failed download2File() Failed download2File() Failed download2File()\n"
			  "download2File(): Error failed curl_easy_perform() function.\n"
			  " Parameter 'filename': <%s>\n"
			  " Remote URL: <%s>\n"
			  " curl_easy_strerror() for result: <%s>\n"
			  " curlErrorMsg contents: <%s>\n"
			  "@@@ end log message @@@\n",
			  filename, currentUrl, curl_easy_strerror(result), curlErrorMsg);

	  writeLogCurl(curlLogtoFP, logBuffer);
	}

	if(toFilePtr){

	  myResult = closeFile(toFilePtr);
	  if(myResult != ztSuccess)
		fprintf(stderr, "download2File(): Error failed closeFile() function.\n");

	  /* we want to return ztFailedDownload; still 2 errors might be related?! **/
	}

	//return ztFailedDownload;
	return getCurlResponseCode(handle);

  } /* end if(result != CURLE_OK) **/

  /* return error when we fail to close it **/
  myResult = closeFile(toFilePtr);
  if(myResult != ztSuccess){
	fprintf(stderr, "download2File(): Error failed closeFile() function.\n");
	return myResult;
  }

  result = curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &responseCode);
  if (result != CURLE_OK){

	responseCode = -1;

	fprintf(stderr, "download2File(): Error failed curl_easy_getinfo() for CURLINFO_RESPONSE_CODE.\n");

    return ztFailedLibCall;
  }

  long   sizeDisk = 0L; /* on disk file size, after download has completed **/
  long   sizeHeader = 0L;

  curl_off_t clSize; /* size at header content-length field : curl_off_t type **/

  curl_off_t dlSize;  /* download size as curl_off_t type **/

  /* get on disk file size, return on failure **/
  myResult = getFileSize(&sizeDisk, filename);
  if(myResult != ztSuccess){
    fprintf(stderr, "download2File(): Error failed getFileSize() function.\n");
	return myResult;
  }

//fprintf(stdout,"download2File(): File size on disk after download is: <%ld> bytes.\n", sizeDisk);

  result = curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &clSize);
  if(result != CURLE_OK){

	fprintf(stderr, "download2File(): Error failed curl_easy_getinfo() for"
			" CURLINFO_LENGTH_DOWNLOAD_T.\n");

    return ztFailedLibCall;
  }
  else {

	sizeHeader = (long) clSize;
  }

  result = curl_easy_getinfo(handle, CURLINFO_SIZE_DOWNLOAD_T, &dlSize);
  if(result != CURLE_OK){

	fprintf(stderr, "download2File(): Error failed curl_easy_getinfo() for "
			"CURLINFO_SIZE_DOWNLOAD_T.\n");

    return ztFailedLibCall;
  }
  else {

	sizeDownload = (long) dlSize;
  }


  if((result == CURLE_OK) && (responseCode == OK_RESPONSE_CODE)){

	if((sizeDisk == sizeHeader) || (sizeDisk == sizeDownload)){

	  /* ALL three numbers should match - for now,
	   * download is okay when fileSize matches at least
	   * one of the two numbers.
	   * Another thing to do is to look inside and verify each
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

		sprintf(logBuffer, "WARNING: DIFFERENT SIZES:\n"
				  "download2File(): Different sizes for sizeHeader and sizeDownload.\n"
				  " sizeHeader is: <%ld> and sizeDownload is: <%ld>\n"
				  " File disk size is: <%ld>\n"
				  " Parameter 'filename': <%s>\n"
				  " Current Remote URL: <%s>\n"
				  " Note that non-named page does not have the header size set.\n"
				  "@@@ end log message @@@\n",
				  sizeHeader, sizeDownload, sizeDisk, filename, currentUrl);

		writeLogCurl(curlLogtoFP, logBuffer);
	  }

	  return ztSuccess;
	}
	else { /* failed fileSize test:  **/

	  if(curlLogtoFP){

		char   logBuffer[PATH_MAX] = {0};

		sprintf(logBuffer, "Failed size test Failed size test Failed size test\n"
				  "download2File(): SUCCESS curl_easy_perform() function result == CURLE_OK.\n"
				  " Response Code == 200 (OK_RESPONSE_CODE)\n"
				  " Parameter filename: <%s>\n"
				  " Current Remote URL: <%s>\n"
				  " No function errors to include. But different sizes ...\n"
				  " sizeHeader is: <%ld> and sizeDownload is: <%ld>\n"
				  " File disk size is: <%ld>\n"
				  "@@@ end log message @@@\n",
				  filename, currentUrl, sizeHeader, sizeDownload, sizeDisk);

		writeLogCurl(curlLogtoFP, logBuffer);
	  }

	  return ztBadSizeDownload;
	}

  } /* end if((result == CURLE_OK) && (responseCode == OK_RESPONSE_CODE)) **/

  return getCurlResponseCode(handle);

} /* END download2File() **/

char *getCurrentURL(){

  CURLUcode  result;
  char   *url = NULL;

  char   *scheme = NULL;
  char   *host = NULL;
  char   *path = NULL;
  char   myUrl[PATH_MAX] = {0};

  result = curl_url_get(parseUrlHandle, CURLUPART_SCHEME, &scheme, 0);
  if (result != CURLUE_OK ) {
    fprintf(stderr, "getCurrentURL(): Error failed curl_url_get() for 'scheme' part.\n");
    return url;
  }

  result = curl_url_get(parseUrlHandle, CURLUPART_HOST, &host, 0);
  if (result != CURLUE_OK ) {
    fprintf(stderr, "getCurrentURL(): Error failed curl_url_get() for 'host' part.\n");

    if(scheme) 	curl_free(scheme);

    return url;
  }

  result = curl_url_get(parseUrlHandle, CURLUPART_PATH, &path, 0);
  if (result != CURLUE_OK ) {
    fprintf(stderr, "getCurrentURL(): Error failed curl_url_get() for 'path' part.\n");

    if(scheme) 	curl_free(scheme);
    if(host)  curl_free(host);

    return url;
  }

  sprintf(myUrl, "%s://%s%s", scheme, host, path);

  url = STRDUP(myUrl);

  if(scheme)
	curl_free(scheme);

  if(host)
	curl_free(host);

  if(path)
    curl_free(path);

  return url;

} /* END getCurrentURL() **/


/* initialQuery(): initials curl handle for query, calls curl_easy_initial() then
 * sets basic common options including the call back function to call.
 * Parameters: serverUrl is curl URL handle returned by initialURL(),
 *                     secToken : character pointer for security token.
 * Note: a server may require user name and password; not handled here.
 * return: CURL handle or NULL on error.
 ****************************************************************************/

CURL * initialQuery (CURLU *serverUrl, char *secToken){

  CURL      *qryHandle = NULL;
  CURLcode  result;


  ASSERTARGS (serverUrl);

  if (sessionFlag == 0){
    fprintf(stderr, "initialQuery(): Error, session not initialized. You must call\n "
	    " initialCurlSession() first and check its return value.\n");
    return qryHandle;
  }

  qryHandle = easyInitial(); /* easyInitial() is defined in curl_func.h as curl_easy_init() */
  if ( ! qryHandle){
    fprintf(stderr, "initialQuery(): curl_easy_init() call failed. Client:: Abort?!\n");
    return qryHandle;
  }

  /* connect CURL handle with CURLU handle */
  result = curl_easy_setopt(qryHandle, CURLOPT_CURLU, serverUrl);
  if(result != CURLE_OK) {
    fprintf(stderr, "initialQuery(): Error failed setopt() : CURLOPT_CURLU.\n");
    easyCleanup(qryHandle);
    qryHandle = NULL;
    return qryHandle;
  }

  /* CURL_USER_AGENT is defined in curl_func.h */
  result = curl_easy_setopt(qryHandle, CURLOPT_USERAGENT, CURL_USER_AGENT);
  if(result != CURLE_OK) {
    fprintf(stderr, "initialQuery(): Error failed setopt() : CURLOPT_USERAGENT.\n");
    easyCleanup(qryHandle);
    qryHandle = NULL;
    return qryHandle;
  }

  result = curl_easy_setopt (qryHandle, CURLOPT_TCP_KEEPALIVE, 1L);
  if(result != CURLE_OK) {
    fprintf(stderr, "initialQuery() failed to set KEEPALIVE option"
	    "{CURLOPT_TCP_KEEPALIVE}: %s\n", curl_easy_strerror(result));
    easyCleanup(qryHandle);
    qryHandle = NULL;
    return qryHandle;
  }

  /* try to get human readable error message */
  result = curl_easy_setopt (qryHandle, CURLOPT_ERRORBUFFER, curlErrorMsg);
  if (result != CURLE_OK) {
    fprintf(stderr, "initialQuery() Error: failed to set ERRORBUFFER option\n"
	    " curl_easy_setopt(.., CURLOPT_ERRORBUFFER, ..) failed: %s\n", curl_easy_strerror(result));
    easyCleanup(qryHandle);
    qryHandle = NULL;
    return qryHandle;
  }

  if (secToken){
    result = curl_easy_setopt(qryHandle, CURLOPT_COOKIE, secToken);
    if(result != CURLE_OK) {
      fprintf(stderr, "initialQuery(): Error failed setopt() : CURLOPT_COOKIE.\n");
      easyCleanup(qryHandle);
      qryHandle = NULL;
      return qryHandle;
    }
  }

  result = curl_easy_setopt(qryHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  if(result != CURLE_OK) {
    fprintf(stderr, "initialQuery()  failed to set WRITEFUNCTION option! "
	    "{CURLOPT_WRITEFUNCTION}: %s\n", curl_easy_strerror(result));
    easyCleanup(qryHandle);
    qryHandle = NULL;
    return qryHandle;
  }

  result = curl_easy_setopt (qryHandle, CURLOPT_HTTPGET, 1L);
  if(result != CURLE_OK) {
    fprintf(stderr, "initialQuery() failed to set HTTPGET option "
	    "{CURLOPT_HTTPGET}: %s\n", curl_easy_strerror(result));
    easyCleanup(qryHandle);
    qryHandle = NULL;
    return qryHandle;
  }

  return qryHandle;

} /* END initialQuery() */

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
 *
 **	400 code 400 code 400 code 400 code 400 code 400 code
 * overpass response code == 400 with malformed query string. Parse error.
 * server did not understand 400
 */

int performQuery (MEMORY_STRUCT *dst, char *whichData, CURL *qHandle, CURLU *srvrHandle) {

  CURLcode   result;
  char       *queryEscaped;
  char       *serverName;
  char       getBuf[PATH_MAX] = {0};

  ASSERTARGS (dst && whichData && qHandle && srvrHandle);

  if (sessionFlag == 0){
    fprintf(stderr, "performQuery(): Error, session not initialized. You must call\n "
	    " curlInitialSession() first and check its return value.\n");
    return ztNoSession;
  }

  if (strlen(whichData) == 0){
    fprintf(stderr, "performQuery(): Error empty (query string), whichData parameter!\n");
    return ztInvalidArg;
  }

  /* check that initialURL() was called, try to get serverName - overhead? NO. */

  result = curl_url_get (srvrHandle, CURLUPART_HOST, &serverName, 0);
  if(result != CURLE_OK) {
    fprintf(stderr, "performQuery(): Error failed curl_url_get() call for HOST name. "
	    "Curl string error: %s\n", curl_easy_strerror(result));
    return result;
  }

  /* need more checking */
  if (serverName == NULL || strlen(serverName) == 0){
    fprintf(stderr, "performQuery(): Error server name NOT set in srvrHandle parameter!"
	    " did you call initialURL()?\n");
    return ztInvalidArg;
  }

  queryEscaped = curl_easy_escape(qHandle, whichData, strlen(whichData));
  if (!queryEscaped){
    printf("performQuery(): Error failed curl_easy_esacpe() function.\n");
    return ztFailedLibCall;
  }

  /* add "data=" prefix to the encoded query string */
  sprintf(getBuf, "data=%s", queryEscaped);

  /* replace query part in URL srvrHandle */
  result = curl_url_set(srvrHandle, CURLUPART_QUERY, getBuf, 0);
  if(result != CURLE_OK) {
    fprintf(stderr, "performQuery(): Error failed curl_url_set() call for query part. "
	    "Curl string error: %s\n", curl_easy_strerror(result));
    return result;
  }

  result = curl_easy_setopt(qHandle, CURLOPT_WRITEDATA, (void *)dst);
  if(result != CURLE_OK) {
    fprintf(stderr, "performQuery() curl_easy_setopt() failed to set WRITEDATA "
	    "{CURLOPT_WRITEDATA}: %s\n", curl_easy_strerror(result));
    return result;
  }

  /* uncomment for debug */
  /* curl_easy_setopt(qHandle, CURLOPT_VERBOSE, 1L); */

  //clearInfo();

  /* get it! */
  result = curl_easy_perform(qHandle);

  //getInfo(qHandle, result);

  /* check for errors */
  if(result != CURLE_OK) {
    fprintf(stderr, "performQuery(): error failed curl_easy_perform() call. String Error: %s\n",
	    curl_easy_strerror(result));

    if (strlen(curlErrorMsg))
      fprintf(stderr, "performQuery(): Curl Perform Error Message: %s\n", curlErrorMsg);
  }
  /*
    else {

    printf("performQuery(): Done with success.  "
    "%lu bytes retrieved\n\n", (unsigned long) dst->size);

    }
  */
  if (queryEscaped)
    curl_free(queryEscaped);

  /* write received data to client opened file if curlRawDataFP is set */
  if (curlRawDataFP) {

    fprintf(curlRawDataFP, "%s", dst->memory);
    fflush(curlRawDataFP);
  }

  return result;

} /* END performQuery() */

/* isOkResponse(): remote server response is okay if first line in the
 * response matches header.
 * this function compares the first "header" string length characters from
 * "responseStr" to "header".
 * Question? : I am hoping this tells me the query was understood by
 * the server; the query syntax was correct.
 *
 * THIS THIS THIS  This should return TRUE or FALSE. FIXME
 ********************************************************************/

int isOkResponse (char *responseStr, char *header){

  ASSERTARGS (responseStr && header);

  int retCode;

  if (strncmp (responseStr, header, strlen(header)) == 0){

    retCode = ztSuccess;
  }
  else {

    fprintf (stderr, "isOkResponse(): Error: Not a valid response. Server may responded "
	     "with an error message! The server response was:\n\n");
    fprintf (stderr, " Start server response below >>>>:\n\n");
    fprintf (stderr, "%s\n\n", responseStr);
    fprintf (stderr, " >>>> End server response This line is NOT included.\n\n");
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

  fprintf(to, "\n- - - - - - - - - - - -Start- Curl Functions Log -Start- - - - - - - - - - - - - - - -\n\n");

  fprintf (to, "%s [%d] writeLogCurl() received message below:\n %s\n", timestamp, (int) myPID, msg);

  fprintf(to, "- - - - - - - - - - - - -End- Curl Functions Log -End- - - - - - - - - - - - - - - - -\n\n");

  return ztSuccess;

} /* END writeLogCurl() **/

/* Note change to download2File() function above.
 *
 * download2FileRetry():
 * calls download2File(), retries when Response Code == 500 ONLY.
 *
 ********************************************************************************/

int download2FileRetry(char *destFile, CURL *handle){

  ASSERTARGS(destFile && handle);

  if (sessionFlag == 0){
    fprintf(stderr, "download2FileRetry(): Error, curl session not initialized. You must call\n "
	    " initialCurlSession() first and check its return value.\n");
    return ztNoSession;
  }

  int   result;
  int   delay = 2 * 30;  /* sleep time in seconds **/

  result = download2File(destFile, handle);

  if(result == ztSuccess)

    return result;

  //if(curlResponseCode == 500)
  if(result == ztResponse500)

	sleep(delay);

  else

	return result;

  /* try again **/
  result = download2File(destFile, handle);
  if(result != ztSuccess)

	fprintf(stderr, "download2FileRetry(): Error failed download2File() for second attempt.\n");

  return result;

} /* END download2FileRetry() **/

/* getCurlResponseCode():
 *
 * FUNCTION IS NOT TO BE CALLED WHEN responseCode == 200.
 *
 * A lot of UNHANDLED codes, log those codes for now.
 */

ZT_EXIT_CODE getCurlResponseCode(CURL *handle){

  CURLcode       result;
  long           responseCode;

  ASSERTARGS(handle);

  if(responseCode == 200)

	return ztSuccess;

  result = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &responseCode);
  if(result != CURLE_OK){

	responseCode = -1;

	fprintf(stderr, "getCurlResponseCode(): Error failed curl_easy_getinfo() for response code! "
			"Code is set to: <-1>\n");

	if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "getCurlResponseCode(): Error failed curl_easy_getinfo() for response code! "
				"Code is set to: <-1>\n");

      writeLogCurl(curlLogtoFP, logBuffer);
    }

	return ztFatalError;
  }
  else if(responseCode == 0){

	fprintf(stderr, "getCurlResponseCode(): Error did NOT receive any response code from server.\n");

	if(curlLogtoFP){
      char   logBuffer[PATH_MAX] = {0};
	  sprintf(logBuffer, "getCurlResponseCode(): Error did NOT receive any response code from server.\n");
      writeLogCurl(curlLogtoFP, logBuffer);
    }

	return ztResponseUnknown;
  }

  switch(responseCode){

  case 301:

	fprintf(stderr, "getCurlResponseCode(): Error received Server Response Code 301.\n Code is for: <%s>\n",
				ztCode2Msg(ztResponse301));

	return ztResponse301;
	break;

  case 400:

	fprintf(stderr, "getCurlResponseCode(): Error received Server Response Code 400.\n Code is for: <%s>\n",
			ztCode2Msg(ztResponse400));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "Response Code 400 Response Code 400 Response Code 400\n"
				  "getCurlResponseCode(): Error received Server Response Code <400>\n Code is for: <%s>\n",
				  ztCode2Msg(ztResponse400));

      writeLogCurl(curlLogtoFP, logBuffer);
    }


	  return ztResponse400;
	  break;

  case 403:

    fprintf(stderr, "getCurlResponseCode(): Error received Server Response Code 403\n Code is for: <%s>\n",
			ztCode2Msg(ztResponse403));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "Response Code 403 Response Code 403 Response Code 403\n"
					  "getCurlResponseCode(): Error received Server Response Code <403>\n Code is for: <%s>\n",
					  ztCode2Msg(ztResponse403));

	  writeLogCurl(curlLogtoFP, logBuffer);
    }

	return ztResponse403;
	break;


  case 404:

    fprintf(stderr, "getCurlResponseCode(): Error received Server Response Code 404.\n Code is for: <%s>\n",
			ztCode2Msg(ztResponse404));

    if(curlLogtoFP){

      char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "Response Code 404 Response Code 404 Response Code 404\n"
			  "getCurlResponseCode(): Error received Server Response Code <404>\n Code is for: <%s>\n",
			  ztCode2Msg(ztResponse404));

	  writeLogCurl(curlLogtoFP, logBuffer);
    }

    return ztResponse404;
	break;

  case 429:

	fprintf(stderr, "getCurlResponseCode(): Error received Server Response Code 429\n Code is for: <%s>\n",
			ztCode2Msg(ztResponse429));

	if(curlLogtoFP){

	  char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "Response Code 429 Response Code 429 Response Code 429\n"
			  "getCurlResponseCode(): Error received Server Response Code <429>\n Code is for: <%s>\n",
			  ztCode2Msg(ztResponse429));

	  writeLogCurl(curlLogtoFP, logBuffer);
	}

	return ztResponse429;
	break;

  case 500:

	fprintf(stderr, "getCurlResponseCode(): Error received Server Response Code 500.\n Code is for: <%s>\n",
			ztCode2Msg(ztResponse500));

	if(curlLogtoFP){

	  char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "Response Code 500 Response Code 500 Response Code 500\n"
			  "getCurlResponseCode(): Error received Server Response Code <500>\n Code is for: <%s>\n",
			  ztCode2Msg(ztResponse500));

	  writeLogCurl(curlLogtoFP, logBuffer);
	}

	return ztResponse500;
	break;

  case 503:

	fprintf(stderr, "getCurlResponseCode(): Error received Server Response Code 503.\n Code is for: <%s>\n",
				ztCode2Msg(ztResponse503));

	if(curlLogtoFP){

	  char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer, "Response Code 503 Response Code 503 Response Code 503\n"
			  "getCurlResponseCode(): Error received Server Response Code <503>\n Code is for: <%s>\n",
			  ztCode2Msg(ztResponse503));

	  writeLogCurl(curlLogtoFP, logBuffer);
	}

	return ztResponse503;
	break;


  default:

	fprintf(stderr, "getCurlResponseCode(): Error unhandled response code. THIS IS THE DEFAULT CASE.\n"
			" RESPONSE CODE IS NOT HANDLED BY THIS FUNCTION. CODE IS: <%ld>.\n", responseCode);

	if(curlLogtoFP){

	  char   logBuffer[PATH_MAX] = {0};

	  sprintf(logBuffer,"getCurlResponseCode(): Error unhandled response code. THIS IS THE DEFAULT CASE.\n"
			" RESPONSE CODE IS NOT HANDLED BY THIS FUNCTION. CODE IS: <%ld>.\n", responseCode);

	  writeLogCurl(curlLogtoFP, logBuffer);
	}

	return ztResponseUnhandled;
	break;

  } /* end switch(curlResponseCode) **/

  fprintf(stderr, "getCurlResponseCode(): Error: THIS IS OUT SIDE OF SWITCH STATEMENT AND SHOULD NOT BE SEEN.\n");

  if(curlLogtoFP){

	char   logBuffer[PATH_MAX] = {0};

	sprintf(logBuffer, "getCurlResponseCode(): Error: THIS IS OUT SIDE OF SWITCH STATEMENT AND SHOULD NOT BE SEEN.\n");

	writeLogCurl(curlLogtoFP, logBuffer);
  }

  return ztUnknownError; /* we do not get here **/

} /* END getCurlResponseCode() **/
