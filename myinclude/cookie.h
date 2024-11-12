/*
 * cookie.h
 *
 *  Created on: Feb 15, 2022
 *      Author: wael
 */

#ifndef COOKIE_H_
#define COOKIE_H_


#include <time.h>

extern int fd2Close;

extern int serverResponse;

/* user may set cookieLogFP for progress/error logging **/
extern FILE *cookieLogFP;

/* geofabrik cookie file has only one single line - they may change this format */
#define MAX_COOKIE_LINES 1

typedef struct COOKIE_FILES_{

  char *cookieFile;
  char *scriptFile;
  char *jsonFile;

} COOKIE_FILES;

typedef struct	COOKIE_ {

/* members we use are included here. Filled in 2 functions:
 *
 *  Filling function           Member
 *  ----------------           -------
 *  parseCookieFile()          token
 *  parseCookieFile()          expitreTimeStr
 *  parseTimeStr()             expireTM
 *  parseTimeStr()             expireSeconds
 *
 *
 *********************************************/

  char  *token;

  char	*expireTimeStr;

  struct tm expireTM; /* expire time 'tm' structure, GMT time **/

  time_t expireSeconds; /* time value in seconds - type 'time_t' ;
                           inverse of 'tm' structure in
                           expireTM member; calculated using
                           makeTimeGMT() function. **/

} COOKIE;

int cookieSetFilenames(COOKIE_FILES *cfiles, SKELETON *dirs);

//int getCookieFile (SETTINGS *settings);

int getCookieFile (MY_SETTING *settings, COOKIE_FILES *cfiles);

int getCookieRetry (MY_SETTING *settings, COOKIE_FILES *cfiles);

int parseCookieFile (COOKIE *dstCookie, const char *filename);

//int parseTimeStr (COOKIE *cookie, char const *str);
int parseTimeStr (struct tm *expireTM, char const *str);

int day2num (char *day);

int month2num (char *month);

//void printCookie(COOKIE *ck);

void fprintCookie(FILE *file, COOKIE *ck);

int isExpiredCookie(COOKIE *ck);

int removeFiles (COOKIE_FILES *cfiles);

int pipeSpawnScript (const char *prog, char * const argList[], char **outputString);

int getResponseCode (int *code, char *msg);

//void logUnseen(SETTINGS *settings, char *msg, char *lastPart);

//int getCookieRetry (SETTINGS *settings);

//int doCookie(SETTINGS *settings);

int doCookie(MY_SETTING *settings, SKELETON *dirs);

char *getCookieToken();

void destroyCookie();

time_t makeTimeGMT(struct tm *tm);

int isCookieFile(char *name);


#endif /* COOKIE_H_ */
