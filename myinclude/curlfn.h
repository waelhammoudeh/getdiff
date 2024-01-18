/*
 * curlfn.h renamed from curl_func.h
 *
 *  Created on: Mar 31, 2021
 *  Author: Wael Hammoudeh
 *
 **********************************************************/

#ifndef CURL_FUNC_H_
#define CURL_FUNC_H_

#ifndef CURLINC_CURL_H
#include  <curl/curl.h>
#endif

#include "ztError.h"

/* DEFAULT_SERVER : use localhost as default server if needed. **/
#ifndef DEFAULT_SERVER
#define DEFAULT_SERVER "http://localhost"
#endif

/* timeout used for queries in seconds - use 0L to disable timeout **/
#define QUERY_TIMEOUT 180L

/* SLEEP_SECONDS : seconds to wait before retry **/
#define SLEEP_SECONDS 30

/* exported variables - user may set */

extern FILE *curlRawDataFP;

extern FILE *curlLogtoFP;

/* exported variables - user may read, READ ONLY */
extern long  sizeDownload;

extern long  responseCode;

extern char  curlErrorMsg[CURL_ERROR_SIZE + 1];

/* curlRawDataFP or curl raw data file pointer:  when set by client query response
 * data will written to that client open file.
 * Client opens the file before a call to performQuery() and closes the file after
 * the function has returned.
 *
 * curlLogtoFP: if set by user, writes selected messages to open file.
 *************************************************************************/

/* To parse URL we use curl_url() which is available since version 7.62.0
 * and curl_url_strerror() which is available since 7.80.0
 * CURLOPT_CONNECT_ONLY this was added in 7.15.2. I had this WRONG as 7.86.0.
 *
 * see CURL_VERSION_BITS(x,y,z) macro in curlver.h
 * #define MIN_CURL_VER 0x073e00u
 *
***********************************************************************/

#define MAJOR_REQ 7
#define MINOR_REQ 80
#define PATCH_REQ 0
#define MIN_CURL_VER ((uint) CURL_VERSION_BITS(MAJOR_REQ,MINOR_REQ,PATCH_REQ))

#define CURL_USER_AGENT "curl/7.80.0"

#define easyInitial() curl_easy_init()
#define easyCleanup(h) curl_easy_cleanup(h)
#define urlCleanup(retValue) curl_url_cleanup(retValue)

#define OK_RESPONSE_CODE   200L

// structure from examples/getinmemory.c - added  typedef
typedef struct MEMORY_STRUCT_  {

	char    *memory;
	size_t	size;

} MEMORY_STRUCT;

typedef enum HTTP_METHOD_ {

	Get = 1, Post
}HTTP_METHOD;

int initialCurlSession(void);

void closeCurlSession(void);

CURLU *initialURL (const char *server);

int isConnCurl(const char *server);

CURL *initialOperation(CURLU *srcUrl, char *secToken);

int download2File(char *filename, CURL *handle, CURLU *parseHandle);

char *getPrefixCURLU(CURLU *parseUrlHandle);

int performQuery(MEMORY_STRUCT *dst, char *whichData, CURL *qHandle, CURLU *srvrURL);

int performQueryRetry(MEMORY_STRUCT *dst, char *whichData, CURL *qHandle, CURLU *srvrURL);

int isOkResponse(char *response, char *header);

int writeLogCurl(FILE *to, char *msg);

int download2FileRetry(char *filename, CURL *handle, CURLU *parseHandle);

ZT_EXIT_CODE responseCode2ztCode(long resCode);

MEMORY_STRUCT *initialMS(void);

void zapMS(MEMORY_STRUCT **ms);


#endif /* CURL_FUNC_H_ */
