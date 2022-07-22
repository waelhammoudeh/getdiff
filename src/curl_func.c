/* curl_func.c :
 *
 * wraps curl library functions I usually use in very few functions.
 * this needs a lot more work.
 * the goal is to have two main functions :
 *   - download2File()
 *   - preformQuery()
 * with the fewest function calls possible.
 *
 * NOTES:
 *  - I use curl URL API; to let libcurl do URL parsing for us. See 'man curl_url'.
 *  - I initial downloads and queries using CURLU *handle.
 *
 *******************************************************************/

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>

#include "curl_func.h"
#include "util.h"
#include "ztError.h"
#include "dList.h"

// global variables
static  CURL       *curl_handle;
static  int        sessionFlag = 0; // global initial flag
static  int        afterPostFlag = 0; // this is ugly! But ...
int                curlResponseCode; /* remote server response code */
int                 downloadSize;
char	           performErrorMsg[CURL_ERROR_SIZE]; /* filled by curl_easy_perform() - can be empty! */
char               *recErrorMsg = NULL; /* copy of above or curl_easy_strerror() */

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

static size_t WriteMemoryCallback (void *contents, size_t size,
                                            size_t nmemb, void *userp) {

  size_t realsize = size * nmemb;
  MEMORY_STRUCT *mem = (MEMORY_STRUCT *) userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */
    printf("WriteMemoryCallback(): Error not enough memory "
    		"(realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

/*** to use curlMemoryDownload() function, you must call initialCurlSession() FIRST.
 *    call closeCurlSession() when done. I have not tested mixing methods.
 *    Para: dst is a pointer to MemoryStrut allocated by the client / caller. Function
 *    allocates memory for memory member, downloaded data is copied there, function
 *    also sets size to the downloaded data byte count.
 *    urlPath: pointer to string including protocol plus the path to remote server; example:
 *      http://localhost/api/interpreter
 *    whichData: character pointer specifying the remote resource - file or script.
 *    method: enumerated number for HTTP method {Get or Post}.
 *    Return: ztSuccess, Numbers of error codes .....
 *    Caller on successful return should copy the data right away.
 */

int curlMemoryDownload(MEMORY_STRUCT *dst, char *urlPath,
		                                char *whichData, HTTP_METHOD method) {

  CURLcode 	res;
  char			*srvrPath;
  char 			*query;
  char			getBuf[LONG_LINE] = {0};
  char 			*curlEscaped = NULL;

  if (sessionFlag == 0){
	  fprintf(stderr, "curlMemoryDownload(): Error, session not initialized. You must call\n "
			     " curlInitialSession() first and check its return value.\n");
	  return ztInvalidUsage;
  }

  if (curl_handle == NULL){
	  fprintf(stderr, "curlMemoryDownload(): Error, curl_handle is NULL. "
			  "Maybe you should abort()!\n");
	  return ztUnknownError;
  }

  ASSERTARGS (dst && urlPath && whichData);

  /** USER SHOULD LIMIT SESSION TO ONE METHOD ONLY - DON'T MIX **/
  if ((method != Get) && (method != Post)) {
	  printf ("curlMemoryDownload(): Error unsupported method specified. \n"
			       "  supported methods are Get or Post.\n\n");
	  return ztInvalidArg;

  }

  dst->memory = malloc(1);
  dst->size = 0;

  srvrPath = urlPath;
  query = whichData;

  /* start common CURLOPTs *******/

  res = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  if(res != CURLE_OK) {
	  fprintf(stderr, "curl_easy_setopt() failed to set WRITEFUNCTION "
   				  "{CURLOPT_WRITEFUNCTION}: %s\n", curl_easy_strerror(res));
	  return res;
  }

  res = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)dst);
  if(res != CURLE_OK) {
	  fprintf(stderr, "curl_easy_setopt() failed to set WRITEDATA "
   				  "{CURLOPT_WRITEDATA}: %s\n", curl_easy_strerror(res));
	  return res;
  }

  res = curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, CURL_USER_AGENT);
  if(res != CURLE_OK) {
	  fprintf(stderr, "curl_easy_setopt() failed to set USERAGENT "
   				  "{CURLOPT_USERAGENT}: %s\n", curl_easy_strerror(res));
	  return res;
  }

  //curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);  // FIXME do not leave here

  // start Get method
  if (method == Get){

	  if (afterPostFlag){

		  res = curl_easy_setopt (curl_handle, CURLOPT_HTTPGET, 1L);
		  if(res != CURLE_OK) {
			  fprintf(stderr, "curl_easy_setopt() failed to set HTTPGET "
		   				  "{CURLOPT_HTTPGET}: %s\n", curl_easy_strerror(res));
			  return res;
		  }

		  afterPostFlag = 0;

	  }
	  /* I escape  (encode) the query part ONLY ... this may not be correct! */
	  curlEscaped = curl_easy_escape(curl_handle, query, strlen(query));
	  if ( curlEscaped == NULL ){
		  printf("curlMemoryDownload(): Error returned by curl_easy_esacpe(). It is NULL!!!\n");
		  return ztGotNull;
	  }

	  sprintf(getBuf, "%s?data=%s", srvrPath, curlEscaped);

	  // turn on TRANSFER_ENCODING
	  res = curl_easy_setopt (curl_handle, CURLOPT_TRANSFER_ENCODING, 1L);
	  if(res != CURLE_OK) {
		  fprintf(stderr, "curl_easy_setopt() failed to set TRANSFER_ENCODING"
	   				  "{CURLOPT_TRANSFER_ENCODING}: %s\n", curl_easy_strerror(res));
		  return res;
	  }

	  res = curl_easy_setopt(curl_handle, CURLOPT_URL, getBuf);
	  if(res != CURLE_OK) {
		  fprintf(stderr, "curl_easy_setopt() failed to set URL "
	   				  "{CURLOPT_URL getBuf}: %s\n", curl_easy_strerror(res));
		  return res;
	  }

  } // end if Get

  if (method == Post){

 	  afterPostFlag = 1;

 	  // tell it to use POST http method -- third parameter set to one
 	  res = curl_easy_setopt (curl_handle, CURLOPT_POST, 1L);
 	  if(res != CURLE_OK) {
 		  fprintf(stderr, "curl_easy_setopt() failed to set POST method "
 				  "{CURLOPT_POST}: %s\n", curl_easy_strerror(res));
 		  return res;
 	  }

 	  // what to POST -- third parameter is the pointer to our query string
 	  res = curl_easy_setopt (curl_handle, CURLOPT_POSTFIELDS, query);
 	  if(res != CURLE_OK) {
 		  fprintf(stderr, "curl_easy_setopt() failed to set POSTFIELD "
 				  "{CURLOPT_POSTFIELDS}: %s\n", curl_easy_strerror(res));

 		  return res;
 	  }

 	  res = curl_easy_setopt(curl_handle, CURLOPT_URL, srvrPath);
 	  if(res != CURLE_OK) {
 		  fprintf(stderr, "curl_easy_setopt() failed to set URL to srvrPath "
 				  "{CURLOPT_URL}: %s\n", curl_easy_strerror(res));

 		  return res;
 	  }
   } // end if Post

  /* get it! */
  res = curl_easy_perform(curl_handle);

  /* check for errors */
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }

  else {

	  printf("curlMemoryDownload(): Done.  "
			  	  "%lu bytes retrieved\n\n", (unsigned long) dst->size);

  }

  if (curlEscaped)
	  curl_free(curlEscaped);

  return ztSuccess;

}

/* initialURL(): calls curl_url() and sets server url to let libcurl do URL
 * parsing for us. Parameter server maybe NULL, should be at least scheme
 * and server name. Caller must call urlCleanup(retValue) when done.
 */
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
}

CURL * initialQuery (CURLU *serverUrl,  // curl parser url CURLU not CURL handle
								   MEMORY_STRUCT *ms){ // caller allocate structure

	/* size_t (* writeFunc) (void *contents, size_t size,
	   size_t nmemb, void *userp), // userp is the pointer inside ms parameter **/

	CURL			*qryHandle = NULL;
	CURLcode 	res;

	ASSERTARGS (serverUrl && ms);

	if (sessionFlag == 0){
		fprintf(stderr, "initialQuery(): Error, session not initialized. You must call\n "
				     " initialCurlSession() first and check its return value.\n");
		return qryHandle;
	}

	qryHandle = easyInitial();
	if ( ! qryHandle){
		fprintf(stderr, "initialQuery(): curl_easy_init() call failed. Client:: Abort?!\n");
		return qryHandle;
	}

	res = setBasicOptions (qryHandle, serverUrl);
	if(res != CURLE_OK) {
		fprintf(stderr, "initialQuery(): Error returned from setBasicOptions().\n");
		easyCleanup(qryHandle);
		qryHandle = NULL;
		return qryHandle;
	}


	return qryHandle;

}

CURLcode setBasicOptions (CURL *qH, CURLU *serverUrl){

	CURLcode res;

	ASSERTARGS (qH && serverUrl);

	res = curl_easy_setopt(qH, CURLOPT_CURLU, serverUrl);
	if(res != CURLE_OK) {
		fprintf(stderr, "setBasicOptions() failed to set URL to srvrPath "
				  "{CURLOPT_CURLU}: %s\n", curl_easy_strerror(res));
		return res;
	}

	res = curl_easy_setopt(qH, CURLOPT_USERAGENT, CURL_USER_AGENT);
	if(res != CURLE_OK) {
		fprintf(stderr, "setBasicOptions() failed to set USERAGENT "
	   				"{CURLOPT_USERAGENT}: %s\n", curl_easy_strerror(res));
		return res;
	}

	res = curl_easy_setopt (qH, CURLOPT_TCP_KEEPALIVE, 1L);
	if(res != CURLE_OK) {
		fprintf(stderr, "setBasicOptions() failed to set KEEPALIVE connection"
				  "{CURLOPT_TCP_KEEPALIVE}: %s\n", curl_easy_strerror(res));
		return res;
	}

	res = curl_easy_setopt (qH, CURLOPT_ERRORBUFFER, performErrorMsg);
	if(res != CURLE_OK) {
		fprintf(stderr, "setBasicOptions() Error: failed to set ERRORBUFFER option\n"
				  " curl_easy_setopt(.., CURLOPT_ERRORBUFFER, ..) failed: %s\n", curl_easy_strerror(res));
		return res;
	}

	/* ztSuccess = CURLE_OK = 0 */
	return CURLE_OK;

}

/* initialSecure() : initial secure download
 * acquire CURL easy handle and sets basic options on it for secure download.
 * returns CURL * handle on success or NULL on failure.
 */
CURL *initialSecure (CURLU *srcUrl, char *secToken){

	CURLcode 	result;
	CURL			*secHandle = NULL;

	ASSERTARGS (srcUrl && secToken);

	if (sessionFlag == 0){
		fprintf(stderr, "initialSecure(): Error, session not initialized. You must call\n "
				     " initialCurlSession() first and check its return value.\n");
		return secHandle;
	}

	secHandle = easyInitial();
	if ( ! secHandle){
		fprintf(stderr, "initialSecure(): curl_easy_init() call failed. Client:: Abort?!\n");
		return secHandle;
	}

	result = curl_easy_setopt(secHandle, CURLOPT_BUFFERSIZE, 102400L);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialSecure(): Error failed setopt() : CURLOPT_BUFFERSIZE.\n");
		easyCleanup(secHandle);
		secHandle = NULL;
		return secHandle;
	}

	/* caller initials CURLU with part or complete URL.
	 * Complete when calling performSecureGet() */
	result = curl_easy_setopt(secHandle, CURLOPT_CURLU, srcUrl);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialSecure(): Error failed setopt() : CURLOPT_CURLU.\n");
		easyCleanup(secHandle);
		secHandle = NULL;
		return secHandle;
	}

	result = curl_easy_setopt(secHandle, CURLOPT_USERAGENT, CURL_USER_AGENT);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialSecure(): Error failed setopt() : CURLOPT_USERAGENT.\n");
		easyCleanup(secHandle);
		secHandle = NULL;
		return secHandle;
	}

	result = curl_easy_setopt(secHandle, CURLOPT_MAXREDIRS, 50L);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialSecure(): Error failed setopt() : CURLOPT_MAXREDIRS.\n");
		easyCleanup(secHandle);
		secHandle = NULL;
		return secHandle;
	}

	result = curl_easy_setopt(secHandle, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialSecure(): Error failed setopt() : CURLOPT_HTTP_VERSION.\n");
		easyCleanup(secHandle);
		secHandle = NULL;
		return secHandle;
	}

	result = curl_easy_setopt(secHandle, CURLOPT_COOKIE, secToken);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialSecure(): Error failed setopt() : CURLOPT_COOKIE.\n");
		easyCleanup(secHandle);
		secHandle = NULL;
		return secHandle;
	}

	result = curl_easy_setopt(secHandle, CURLOPT_TCP_KEEPALIVE, 1L);
	if(result != CURLE_OK) {
		fprintf(stderr, "initialSecure(): Error failed setopt() : CURLOPT_TCP_KEEPALIVE.\n");
		easyCleanup(secHandle);
		secHandle = NULL;
		return secHandle;
	}

	return secHandle;

} /* END initialSecure() */

/* download2File():
 *    - toFilePtr : FILE pointer to an open file.
 *    - handle : pointer to an initial CURL handle
 *
 * returns: CURLcode result from curl_easy_perform(), ztParseError.
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

	/* zero out performErrorMsg on each call before curl_easy_perform()*/
	performErrorMsg[0] = 0;

	result = performDownload (handle);
	if (result != CURLE_OK)

		fprintf(stderr, "download2File(): Error returned from performDownload().\n");

	return result;
}

/* performDownload(): calls curl_easy_perform() and then getinfo() after that.
 * converts double to long when needed.
 *
 * returns : CURLcode returned from curl_easy_peform(), ztParseError.
 *
 **************************************************************************/

int performDownload (CURL *handle){

	CURLcode     ret, performResult;
	double         contSizeDbl; // content size as double
	double         rcvdSizeDbl; // received size as double
	long	             contSizeLong = 0L; // content size as long
	long	             rcvdSizeLong = 0L; // received size as long
	long             responseCode;
	int                result;

	ASSERTARGS (handle);

	/* set global curlResponseCode before curl_easy_perform() */
	curlResponseCode = 0;
	contSizeDbl = 0.0;
	rcvdSizeDbl = 0.0;
	downloadSize = 0;

	performResult = curl_easy_perform(handle);

	/* on failure try to get performErrorMsg */
	if (performResult != CURLE_OK){

		size_t length = strlen(performErrorMsg);
		if (length)
			recErrorMsg = strdup (performErrorMsg);
		else
			recErrorMsg = strdup (curl_easy_strerror(performResult));
	}
	/* NOTE: FIXME
	 * recErrorMsg may STILL be NULL - memory allocate not checked above */

	/* getinfo() returns : CURLE_OK or CURLE_UNKNOWN_OPION */

	ret = curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &responseCode);
	if ( ret != CURLE_OK && performResult != CURLE_OK){
		fprintf(stderr, "performDownload(): Error: \n"
				" failed curl_easy_perform() AND curl_easy_getinfo (.. ,CURLINFO_RESPONSE_CODE, ..) not supported!\n");
		return performResult;
	}

	curlResponseCode = (int) responseCode;

	if (responseCode != OK_RESPONSE_CODE)
		fprintf(stdout, "performDownload(): WARNING response code was not OKAY.\n "
				"CODE: [%ld]\n", responseCode);

	ret = curl_easy_getinfo (handle, CURLINFO_SIZE_DOWNLOAD, &rcvdSizeDbl);
	if(ret != CURLE_OK && performResult != CURLE_OK){
		fprintf(stderr, "performDownload(): Error:\n"
				"  failed curl_easy_perform() AND curl_easy_getinfo() failed for DOWNLOAD SIZE:\n");
		return performResult;
	}

	result = convDouble2Long(&rcvdSizeLong, rcvdSizeDbl);
	if (result != ztSuccess){
		fprintf(stderr, "performDownload(): Error converting double to long for Received download.\n");
		return ztParseError;
	}

	downloadSize = (int) rcvdSizeLong;

	/* CURLINFO_CONTENT_LENGTH_DOWNLOAD returns -1 if size is unknown
	 * CURLINFO_CONTENT_LENGTH_DOWNLOAD_T returns "-nan" if size is unknown
	 ********************************************************************************/

	ret = curl_easy_getinfo (handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contSizeDbl);
	if(ret != CURLE_OK)
		fprintf(stdout, "performDownload(): WARNING: "
				"curl_easy_getinfo(.., CURLINFO_CONTENT_LENGTH_DOWNLOAD, ..) not supported!\n");

	if 	(contSizeDbl < 0.0) {
		fprintf(stdout, "performDownload(): WARNING: "
						"curl_easy_getinfo(.., CURLINFO_CONTENT_LENGTH_DOWNLOAD, ..) \n"
						" reported Unknown Content Size\n");
	}
	else {
		if (contSizeDbl != rcvdSizeDbl) { /* call convert only if not the same */
			result = convDouble2Long(&contSizeLong, contSizeDbl);
			if (result != ztSuccess){
				fprintf(stderr, "performDownload(): Error converting double to long for Content size.\n");
				return ztParseError;
			}
		}
	}

	fprintf (stdout, "performDownload(): Download completed, received file size : <%ld>\n",
			   rcvdSizeLong);

	//return ztSuccess;
	return performResult;

} /* END performDownload() */

/* initialDownload() : initial download, if secToken not null; secure download!
 * acquire CURL easy handle and sets basic options on it for download.
 * returns CURL * handle on success or NULL on failure.
 */
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
	 * Complete when calling performSecureGet() */
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
