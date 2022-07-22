/*
 * curl_func.h
 *
 *  Created on: Mar 31, 2021
 *      Author: wael
 */

#ifndef CURL_FUNC_H_
#define CURL_FUNC_H_

#ifndef CURLINC_CURL_H
#include  <curl/curl.h>
#endif

#include "dList.h"

/* exported variables */
extern int curlResponseCode;
extern int downloadSize;
extern char  *recErrorMsg;

/* To parse URL we use curl_url() which is available since version 7.62.0
 * and curl_url_strerror() which is available since 7.80.0
    see CURL_VERSION_BITS(x,y,z) macro in curlver.h
#define MIN_CURL_VER 0x073e00u
***********************************************************************/
#define MIN_CURL_VER ((uint) CURL_VERSION_BITS(7,80,0))

#define CURL_USER_AGENT "curl/7.80.0"

#define easyInitial() curl_easy_init()
#define easyCleanup(h) curl_easy_cleanup(h)
#define urlCleanup(retValue) curl_url_cleanup(retValue)

#define OK_RESPONSE_CODE	200L
// structure from examples/getinmemory.c - added  typedef
typedef struct MEMORY_STRUCT_  {

	char		*memory;
	size_t	size;
} MEMORY_STRUCT;

typedef enum HTTP_METHOD_ {

	Get = 1, Post
}HTTP_METHOD;

int initialCurlSession(void);

void closeCurlSession(void);

int curlMemoryDownload(MEMORY_STRUCT *dst, char *urlPath,
		                                char *whichData, HTTP_METHOD method);

CURLU * initialURL (char *server);

CURLcode setBasicOptions (CURL *qH, CURLU *serverUrl);

CURL *initialSecure (CURLU *srcUrl, char *secToken);

CURL *initialDownload (CURLU *srcUrl, char *secToken);

int download2File (FILE *toFilePtr, CURL *handle);

int performDownload (CURL *handle);

#endif /* CURL_FUNC_H_ */
