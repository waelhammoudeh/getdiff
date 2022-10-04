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


/* exported variables - read only */
extern int curlResponseCode;
extern int downloadSize;
extern char  *recErrorMsg;

/* exported variable for raw data file pointer, when set by client raw query
 * result from remote server is written to that open file.
 *************************************************************************/
extern FILE *rawDataFP;

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

CURLU * initialURL (char *server);

CURL *initialDownload (CURLU *srcUrl, char *secToken);

int download2File (FILE *toFilePtr, CURL *handle);

CURL * initialQuery (CURLU *serverUrl, char *secToken);

int performQuery (MEMORY_STRUCT *dst, char *whichData, CURL *qHandle, CURLU *srvrURL);

void clearInfo (void);

void getInfo (CURL *handle, CURLcode performResult);

int isOkResponse (char *response, char *header);

#endif /* CURL_FUNC_H_ */
