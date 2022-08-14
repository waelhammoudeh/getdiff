/* curl_func.c :
 *
 * There are a lot of settings available in libcurl, the functions in this file
 * bundle curl library functions I usually use in very few functions.
 * This is work in progress and may change.
 * The two main functions are:
 *   - download2File()
 *   - performQuery()
 *
 * NOTES:
 *  - I use curl URL API; to let libcurl do URL parsing for us. See 'man curl_url'.
 *  - I initial downloads and queries using CURLU *handle.
 *  - user gets 2 handles (pointers); CURLU and CURL handles.
 *    the first with initialURL() and the second with initialDownload() or
 *    initialQuery().
 *  - using CURLU handle, libcurl enables user to change / set URL whole or part.
 *
 *******************************************************************/

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>

#include "curl_func.h"
#include "util.h"
#include "ztError.h"

/* WriteMemoryCallback() function is only available in this file */
static size_t WriteMemoryCallback (void *contents,
		                                                       size_t size,
															   size_t nmemb,
															   void *userp);

/* global variables */

int         sessionFlag = 0; /* initial flag - private */

int         curlResponseCode; /* remote server response code - exported */

int         downloadSize;         /* actual received byte count - exported */

char	    performErrorMsg[CURL_ERROR_SIZE + 1];
            /* filled by curl_easy_perform() - can be empty!
                set with option CURLOPT_ERRORBUFFER - private */

char        *recErrorMsg = NULL; /* copy of above or curl_easy_strerror() - exported */

/* initialCurlSession(): calls curl_global_init() after checking version.
 * first function to call, needed only once in a session.
 *****************************************************************/
int initialCurlSession(void){

	CURLcode	result;
	curl_version_info_data *verInfo; /* script or auto tools maybe?? ****/

	if (sessionFlag) // call me only once

		return ztSuccess;

	verInfo = curl_version_info(CURLVERSION_NOW);
	if (verInfo->version_num < MIN_CURL_VER){

		fprintf (stderr, "ERROR: Required \"libcurl\" minimum version is: 7.80.0. Aborting.\n");
		return ztInvalidUsage;
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

/* initialURL(): calls curl_url() and sets server url to let libcurl do URL
 * parsing for us. Parameter server maybe NULL, should be at least scheme
 * and server name.
 * Return: CURLU *, curl URL handle. Or NULL on error.
 * Caller must call urlCleanup(retValue) when done.
 ****************************************************************************/
CURLU * initialURL (char *server){

	CURLU *retValue = NULL;
	CURLUcode result;

	if (sessionFlag == 0){
		fprintf(stderr, "initialURL(): Error, session not initialized. You must call\n "
				     " initialCurlSession() first and check its return value.\n");
		return retValue;
	}

	retValue = curl_url();

	if (retValue == NULL)

		return retValue;

	if (server){
	/* caller should check connection first. checkURL() in network.c file */

		result = curl_url_set(retValue, CURLUPART_URL, server, 0);
		if (result != CURLUE_OK){
			curl_url_cleanup(retValue);
			retValue = NULL;
			fprintf(stderr, "curl_url_set() failed. Curl error message: {%s} for server: <%s>\n",
			            curl_url_strerror(result), server);
		}
	}

	return retValue;

} /* END initialURL() */

/* initialDownload() : initial download, if secToken is not null then we have
 * secure download! function connects CURLU and CURL handles.
 * acquire CURL easy handle and sets basic options on it for download.
 * returns CURL * handle on success or NULL on failure.
 ************************************************************************/
CURL *initialDownload (CURLU *srcUrl, char *secToken){

	CURLcode 	result;
	CURL			*dwnldHandle = NULL;

	//ASSERTARGS (srcUrl && secToken); -- allow NULL for secToken
	ASSERTARGS (srcUrl);

	if (sessionFlag == 0){
		fprintf(stderr, "initialDownload(): Error, curl session not initialized. You must call\n "
				     " initialCurlSession() first and check its return value.\n");
		return dwnldHandle;
	}

	dwnldHandle = easyInitial();
	if ( ! dwnldHandle){
		fprintf(stderr, "initialDownload(): curl_easy_init() call failed. Client:: Abort?!\n");
		return dwnldHandle;
	}

	result = curl_easy_setopt(dwnldHandle, CURLOPT_BUFFERSIZE, 102400L);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialDownload(): Error failed setopt() : CURLOPT_BUFFERSIZE.\n");
		easyCleanup(dwnldHandle);
		dwnldHandle = NULL;
		return dwnldHandle;
	}

	/* caller initials CURLU with part or complete URL.
	 * this call ties URL with curl handle. */
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

	return dwnldHandle;

} /* END initialDownload() */

/* download2File():
 *    - toFilePtr : FILE pointer to an open file. user opens and closes file.
 *    - handle : pointer to an initial CURL handle
 *
 * returns: CURLcode result from curl_easy_perform().
 *
 */
int download2File (FILE *toFilePtr, CURL *handle){

	CURLcode	result;

	ASSERTARGS (toFilePtr && handle);

	result = curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) toFilePtr);
	if (result != CURLE_OK){
		fprintf(stderr, "download2File(): error failed curl_easy_setopt() CURLOPT_WRITEDATA.\n");
		return result;
	}

	clearInfo();

	result = curl_easy_perform(handle);

	getInfo (handle, result);

	if (result != CURLE_OK)

		fprintf(stderr, "download2File(): Error returned from curl_easy_perform().\n");

	return result;
}

/* initialQuery(): initials curl handle for query, calls curl_easy_initial() then
 * sets basic common options including the call back function to call.
 * Parameters: serverUrl is curl URL handle returned by initialURL(),
 *                     secToken : character pointer for security token.
 * Note: a server may require user name and password; not handled here.
 * return: CURL handle or NULL on error.
 ****************************************************************************/

CURL * initialQuery (CURLU *serverUrl, char *secToken){

	CURL			*qryHandle = NULL;
	CURLcode 	result;


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
	result = curl_easy_setopt (qryHandle, CURLOPT_ERRORBUFFER, performErrorMsg);
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

	// turn on TRANSFER_ENCODING
/*	result = curl_easy_setopt (qryHandle, CURLOPT_TRANSFER_ENCODING, 1L);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialQuery() failed to set TRANSFER_ENCODING "
	   				  "{CURLOPT_TRANSFER_ENCODING}: %s\n", curl_easy_strerror(result));
		easyCleanup(qryHandle);
		qryHandle = NULL;
		return qryHandle;
	}
*/
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
 */

int performQuery (MEMORY_STRUCT *dst, char *whichData, CURL *qHandle, CURLU *srvrHandle) {

	CURLcode	result;
	char				*queryEscaped;

	char				*serverName;
	char				getBuf[LONG_LINE] = {0};

	ASSERTARGS (dst && whichData && qHandle && srvrHandle);

	if (sessionFlag == 0){
		fprintf(stderr, "performQuery(): Error, session not initialized. You must call\n "
				     " curlInitialSession() first and check its return value.\n");
		return ztInvalidUsage;
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
	if ( queryEscaped == NULL ){
		printf("performQuery(): Error returned by curl_easy_esacpe(). It is NULL!!!\n");
		return ztGotNull;
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

	clearInfo();

	  /* get it! */
	result = curl_easy_perform(qHandle);

	getInfo(qHandle, result);

	/* check for errors */
	if(result != CURLE_OK) {
		fprintf(stderr, "performQuery(): error failed curl_easy_perform() call. String Error: %s\n",
	                            curl_easy_strerror(result));

		if (strlen(performErrorMsg))
			fprintf(stderr, "performQuery(): Curl Perform Error Message: %s\n", performErrorMsg);
	}

	else {

		printf("performQuery(): Done with success.  "
				  	  "%lu bytes retrieved\n\n", (unsigned long) dst->size);

	}

	if (queryEscaped)
		curl_free(queryEscaped);

	return result;

} /* END performQuery() */

/* clearInfo(): sets global info variables to zero,
 *  call just before curl_easy_perform()
 * *********************************************************/
void clearInfo(void){

	curlResponseCode = 0;
	downloadSize = 0;
	performErrorMsg[0] = 0;
	recErrorMsg = NULL;

	return;
}

/* getInfo(): gets info from curl on handle,
 * call immediately after curl_easy_perform()
 ********************************************************/
void getInfo (CURL *handle, CURLcode performResult){

	CURLcode     result;
	int                 myResult;

	double         rcvdSizeDbl; /* received size as double - received byte count */
	long	             rcvdSizeLong = 0L; /* received size as long - after conversion */
	long             responseCode = 0L;


	ASSERTARGS (handle);


	/* on failure try to get performErrorMsg */
	if (performResult != CURLE_OK){

		size_t length = strlen(performErrorMsg);
		if (length)
			recErrorMsg = strdup (performErrorMsg);
		else
			recErrorMsg = strdup (curl_easy_strerror(performResult));
	}
	/* FIXME: recErrorMsg may STILL be NULL - memory allocate not checked above */

	/* curl_easy_getinfo() returns : CURLE_OK or CURLE_UNKNOWN_OPION */

	result = curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &responseCode);
	if ( result != CURLE_OK && performResult != CURLE_OK){
		fprintf(stderr, "getInfo(): Error: \n"
				" failed curl_easy_perform() AND curl_easy_getinfo (.. ,CURLINFO_RESPONSE_CODE, ..) not supported!\n");
		// return;
	}

	if (responseCode)

		curlResponseCode = (int) responseCode;

	if (responseCode != OK_RESPONSE_CODE)
		fprintf(stdout, "getInfo(): WARNING response code was not OKAY.\n "
				"CODE: [%ld]\n", responseCode);

	result = curl_easy_getinfo (handle, CURLINFO_SIZE_DOWNLOAD, &rcvdSizeDbl);
	if(result != CURLE_OK && performResult != CURLE_OK){
		fprintf(stderr, "getInfo(): Error:\n"
				"  failed curl_easy_perform() AND curl_easy_getinfo() failed for DOWNLOAD SIZE:\n");
		/* FIXME: I need to think this over */
		return;
	}

	myResult = convDouble2Long(&rcvdSizeLong, rcvdSizeDbl);
	if (myResult != ztSuccess){
		fprintf(stderr, "getInfo(): Error converting double to long for Received download."
				" received size as double: <%f>\n", rcvdSizeDbl);
	}

	if (rcvdSizeLong)

		downloadSize = (int) rcvdSizeLong;

	return;

} /* END getInfo() */
