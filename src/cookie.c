/*
 * cookie.c
 *
 *  Created on: Feb 15, 2022
 *      Author: wael
 */
/* LICENSE.md file from github.com/geofabrik account
   Copyright 2018 Geofabrik GmbH

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   ** End LICENSE.md **/

#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>

#include "getdiff.h"
#include "util.h"
#include "ztError.h"
#include "cookie.h"
#include "fileio.h"

#define LOG_UNSEEN
//#undef LOG_UNSEEN


static COOKIE *cookie = NULL; /* private global variable **/

int serverResponse = 0;


/* getCookieFile (): function retrieves OSM login cookie via "geofabrik" web server,
 *   function stores the cookie in a text file on disk.
 *
 *   Function runs "oauth_cookie_client.py" script to retrieve the cookie. Script behaviour
 *   is summarized in pipeSpawnScript () function below. Also see repository:
 *   https://github.com/geofabrik/sendfile_osm_oauth_protector/blob/master/doc/client.md
 *   for more information about the script.
 *
 *   Function calls fork() then runs the script as the child process in a pipe
 *   with redirection of standard error to capture error messages by the
 *   parent process. In case of error; function calls  getResponseCode () function
 *   to parse error message for server response code.
 * Script error message example:
 *   POST https://www.openstreetmap.org/oauth/authorize, received HTTP code 403 but expected 200
 *
 *	In case of script failure; function sets global variable (int serverResponse) to
 *	received Response Code when possible. If we fail to parse error message for
 *	Response Code then function sets (int serverResponse) to zero.
 *	User (caller) should check serverResponse value on failure.
 *
 *
 * Geofabrik \Response codes I know:
 *  403 --> invalid user or password
 *  429 --> too many requests
 *  500 --> internal server error
 *
 *	check written cookie file - can not be empty.
 *
 * Return:
 *   ztSuccess,
 *   ztPyExecNotFound : could not find python3 executable.
 *   ztCreateFileErr,
 *   ztWriteError,
 *   ztUnusableFile : failed IsArgUsableFile() call.
 *   ztGotNull,
 *   ztEmptyString,
 *   ztUnrecognizedMsg : unrecognized error message from script.
 *   ztMemoryAllocate,
 *   ztBadResponse : parse error, failed to convert string to long.
 *   ztHighResponse : server response code > 599
 *   ztFailedSysCall,
 *   ztChildProcessFailed : exit code for script was not EXIT_SUCCESS or (0).
 *
 * getCookieFile(): function downloads cookie file from "geofabrik" internal
 * server using the python script provided by "geofabrik". The script is executed
 * in a pipe with the function pipeSpawnScript() to redirect STDERROR output to
 * capture the script error messages.
 *
 * Command line to run the script: (arguments list - our array below)
 *  $ /usr/bin/python3 oauth_cookie_client.py -s settings.json -o geofabrikCookie.txt
 *
 **********************************************************************************/

int getCookieFile (SETTINGS *settings){

  char  *argsList[5]; /* array of 5 pointers to character strings - uninitialized **/

  char  *pyExec = "/usr/bin/python3"; /* python executable **/

  char  tmpBuf[1024] = {0}; /* buffer to build strings **/

  int   result;

  char  *scriptErrorMessage; /* pointer to error message from script -
                                this gets filled by pipeSpawnScript() **/

  char  *markerStr = "received HTTP code"; /* included in script error message -
                                              used to parse the error message */
  char  *lastPart = NULL;

  int   responseCode = 0; /* only change when we get from getResponseCode() */

  int   scriptResult; /* returned result from pipeSpawnScript() */

  /* do not allow NULL pointers **/
  ASSERTARGS (settings && settings->scriptFile && settings->jsonFile);

  /* global serverResponse is set if we can get it **/
  serverResponse = 0;

  /* check python executable **/
  result = isExecutableUsable(pyExec);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isExecutableUsable() function for: <%s>.\n",
            progName, pyExec);
    fprintf(stderr," function failed for: <%s>\n", ztCode2Msg(result));
    return result;
  }

  /* write script file in our working directory **/
  result = writeScript(settings->scriptFile);
  if (result != ztSuccess){
    fprintf(stderr, "%s: Error failed to write script to file.\n", progName);
    return result;
  }

  /* write JSON setting file in our working directory **/
  result = writeJSONfile(settings->jsonFile, settings->usr, settings->pswd);
  if (result != ztSuccess){
    fprintf(stderr, "%s: Error failed to write JSON settings file.\n", progName);
    removeFile(settings->scriptFile);
    return result;
  }

  /* set the pointers in our argument list array - command line arguments. **/
  argsList[0] = STRDUP(pyExec);

  argsList[1] = STRDUP(settings->scriptFile);

  sprintf (tmpBuf, "-s%s", settings->jsonFile);
  argsList[2] = STRDUP(tmpBuf);

  sprintf (tmpBuf, "-o%s", settings->cookieFile);
  argsList[3] = STRDUP(tmpBuf);

  /* correct; no space between switch and its argument in above 2 sprintf() calls. **/

  argsList[4] = NULL; /* last pointer must be NULL **/

  /* run the script in a pipe **/
  scriptResult = pipeSpawnScript (pyExec, argsList, &scriptErrorMessage);

  /* remove temporary files - no longer needed **/
  result = removeFile(settings->scriptFile);
  if (result != ztSuccess)
    fprintf(stderr, "%s: Warning failed to remove temporary script file(s)!\n", progName);

  result = removeFile(settings->jsonFile);
  if (result != ztSuccess)
    fprintf(stderr, "%s: Warning failed to remove temporary JSON file(s)!\n", progName);

  /* examine scriptResult now;
   * script writes error messages to STDERR,
   * pipeSpawnScript() gets this in our third parameter,
   * it allocates memory and fills string when possible */

  if (scriptResult == ztSuccess){

    /* check cookie file status **/
    result = isFileUsable(settings->cookieFile);
    if(result == ztSuccess){

      serverResponse = 200; /* set global variable - we assume this! **/
      return ztSuccess; /* script run okay AND cookie file is okay **/
    }

    else

      return ztUnknownError; /* script was okay but file was not **/
  }

  /* scriptResult != ztSuccess; failed script result **/
  if(scriptErrorMessage == NULL){


#ifdef LOG_UNSEEN
    logUnseen (settings, "No error message: (scriptErrorMessage == NULL)", "NULL");
#endif

    fprintf (stderr, "%s: Error failed script with no error message"
	     " (message pointer = NULL) in getCookieFile().\n", progName);

    return scriptResult;
  }

  /* we have an error message, try to get response code **/
  lastPart = strstr(scriptErrorMessage, markerStr);

  if( ! lastPart ){

    /* error message doesn't include markerStr -> unrecognized message
     * for now we log such error messages */

#ifdef LOG_UNSEEN
    sprintf (tmpBuf, "UNRECOGNIZED message: <%s>", scriptErrorMessage);
    logUnseen (settings, tmpBuf, "No lastPart");
#endif

    fprintf(stderr, "%s: Error failed script with UNRECOGNIZED server error"
            " message in getCookieFile()\n "
            " Server Error Message: <%s>.\n", progName, scriptErrorMessage);

    return scriptResult;

  }

  else { /* error massage has lastPart, try to get response code **/

    result = getResponseCode (&responseCode, lastPart);
    if (result == ztSuccess){

      /* set global variable: 'serverResponse' **/
      serverResponse = responseCode;

      switch(responseCode){

      case 403:

	fprintf(stderr, "%s: Error received server response code 403; invalid credentials.\n"
		"Wrong user name or password for OSM account.\n", progName);

	return ztResponse403;
	break;

      case 429:

	fprintf(stderr, "%s: Error received server response code 429; \"too many requests error.\"\n"
		"Please do not use this program for some time - 2 hours at least.\n\n"
		"This program has a maximum of 30 change files and their state files per session.\n"
		"That is a total of 60 files per session which should not exceed Geofabrik.de limits.\n"
		"Geofabrik provide this free service to you and I, please do not abuse their server\n"
		"with too many requests in a short period of time. This maximum is set to avoid server\n"
		"abuse in the first place. Geofabrik free services - like a lot of free services - have rules\n"
		"and consequences for abuse.\n"
		"Again please do not abuse this free service.\n", progName);

	return ztResponse429;
	break;

      case 500:

	fprintf(stderr, "%s: Error received server response code 500; internal server error.\n"
		"One of the two servers - geofabrik.de or openstreetmap.org - might be busy or down.\n",
		progName);

	return ztResponse500;
	break;

      default:

	fprintf(stderr, "%s: Error received server response code < %d > from server.\n"
		"This code could be from either servers - geofabrik.de or openstreetmap.org.\n"
		"This code is not handled by this program.\n", progName, responseCode);

	return ztResponseUnknown;
	break;

      } /* end switch(responseCode) **/

      return scriptResult;

    }
    else{ /* failed to get response code from error message **/

#ifdef LOG_UNSEEN
      sprintf (tmpBuf, "Failed to get response code from error message: <%s>", scriptErrorMessage);
      logUnseen (settings, tmpBuf, lastPart);
#endif

      return scriptResult;

    }

  }

  return ztSuccess;

} /* END getCookieFile() */

int day2num (char *day){

  /* returns an integer number for the abbreviated day
   * Sun -> 0, Mon -> 1 ... Sat -> 6
   * returns -1 for invalid string argument */

  struct day2Num {char *str; int num;};

  struct day2Num myTable[] = {
    {"Sun", 0},
    {"Mon", 1},
    {"Tue", 2},
    {"Wed", 3},
    {"Thu", 4},
    {"Fri", 5},
    {"Sat", 6},
    {NULL, 0}};

  ASSERTARGS (day);

  int	i = 0;
  while(myTable[i].str){

    if (strcmp(myTable[i].str, day) == 0)

      return myTable[i].num;

    i++;
  }

  return (-1);
}

int month2num (char *month){

  /* returns an integer number for the abbreviated month
   * Jan -> 0, Feb -> 1 ... Dec -> 11
   * returns -1 for invalid string argument */

  struct month2Num {char *str; int num;};

  struct month2Num myTable[] = {
    {"Jan", 0},
    {"Feb", 1},
    {"Mar", 2},
    {"Apr", 3},
    {"May", 4},
    {"Jun", 5},
    {"Jul", 6},
    {"Aug", 7},
    {"Sep", 8},
    {"Oct", 9},
    {"Nov", 10},
    {"Dec", 11},
    {NULL, 0}};

  ASSERTARGS (month);

  int	i = 0;
  while(myTable[i].str){

    if (strcmp(myTable[i].str, month) == 0)

      return myTable[i].num;

    i++;
  }

  return (-1);
}

void printCookie(COOKIE *ck){

  ASSERTARGS (ck);

  printf("printCookie(): Cookie members are:\n\n");

  if (ck->token)
    printf("Login Token is: <%s>\n\n", ck->token);
  else
    printf("Login Token is NOT set.\n");

  if(ck->expireTimeStr)
    printf("expireTimeStr is: <%s>\n\n", ck->expireTimeStr);
  else
    printf("expireTimeStr is NOT set.\n");

  /* TODO: incomplete FIXME **/

  return;
}

/* isExpiredCookie(): return TRUE if cookie expires within the next 2 hours 
 * from current time.
 *
 * Cookie expire time is checked only at program startup.
 * We do not retrieve a new cookie if it expires while running.
 * Two hours [120 - 179 minutes] is more than enough time to download
 * change files even with the slowest internet connection; I did not
 * do any testing for this assumption!
 *
 **********************************************************************/
int isExpiredCookie(COOKIE *ck){

  time_t  currentGMT;
  int     SEC_PER_HOUR = 60 * 60;
  int     hours2Expire;

  ASSERTARGS(ck);

  currentGMT = time(&currentGMT);

  hours2Expire = (ck->expireSeconds - currentGMT) / SEC_PER_HOUR;

  /**
  if(hours2Expire > 0)

    fprintf(stdout,"isExpiredCookie(): Cookie expires in: <%d> hour(s).\n", hours2Expire);

  else

    fprintf(stdout,"isExpiredCookie(): Cookie expired: <%d> hour(s) ago.\n", (-1 * hours2Expire));
  ********/

  if(hours2Expire < 2)

    return TRUE;

  return FALSE;

} /* END isExpiredCookie() **/

/* parseCookieFile():
 * opens and read filename, parses read string filling COOKIE structure members.
 * filename: string character pointer to file with cookie string.
 * dstCookie : a pointer to COOKIE structure allocated by caller.
 *
 * File is expected to have ONE single line.
 *
 * Return:
 *  ztInvalidArg if file has more than one line.
 *  ztOpenFileError
 *  ztMemoryAllocate
 *  isFileUsable() failed result.
 *
 *
 **************************************************************************/

int parseCookieFile (COOKIE *dstCookie, const char *filename){

  /* example short cookie file -- login-token part was cut short to fit here */
  /* gf_download_oauth="login|2018-04-12|mmPsXFi3-ftGnxdDpv_pI-CVXmmZDi6SU_vNgpuEsl4c0NK_w=="; expires=Wed, 16 Feb 2022 17:51:27 GMT; HttpOnly; Path=/; Secure 	*/
  /* my fields:
   *  - fileMarker : "gf_download_oauth=\"login|"
   *  - loginToken : "2018-04-12|mmPsXFi3-ftGnxdDpv_pI-CVXmmZDi6SU_vNgpuEsl4c0NK_w==\""
   *  - expireToken: "expires=Wed, 16 Feb 2022 17:51:27 GMT"
   *  - acceptProto: "HttpOnly;"
   *  - pathToken  : "Path=/;"
   *  - sFlagToken : "Secure"
   */

  int   result;
  FILE  *filePtr;
  char  *theLine;
  char  tmpBuf[PATH_MAX] = {0}; /* all are zeros **/
  int   numLines = 0;
  char  *semicol = ";";

  char  *loginToken,
        *expireToken,
        *acceptProto, /* *formatToken, **/
        *pathToken,
        *sFlagToken;

  char  *timeStr;

  char  *fileMarker = "gf_download_oauth=\"login|";
  char  *string;

  ASSERTARGS (dstCookie && filename);

  result = isFileUsable(filename);
  if (result != ztSuccess){
    fprintf(stderr, "%s: Error parseCookieFile() filename argument <%s> not usable.\n",
	    progName, filename);
    return result;
  }

  errno = 0;
  filePtr = fopen ( filename, "r");
  if ( filePtr == NULL){
    fprintf (stderr, "%s: Error parseCookieFile() could not access file! <%s>\n",
	     progName, filename);
    printf("System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }


  /* cookie file has exactly one line, size is well less than PATH_MAX length,
   * return error when more lines are found **/
  while (fgets(tmpBuf, PATH_MAX - 1, filePtr)){

    numLines++;

    if (numLines > MAX_COOKIE_LINES){

      fprintf (stderr, "%s: Error parseCookieFile() found multiple lines: [%d] in cookie file: <%s>\n",
	       progName, numLines, filename);
      fclose(filePtr);
      return ztInvalidArg;
    }

  } /* end while() **/

  fclose(filePtr);

  /* remove line-feed - fgets() keeps */
  if(tmpBuf[strlen(tmpBuf) - 1] == '\n')

    tmpBuf[strlen(tmpBuf) - 1] = '\0';

  theLine = (char *) malloc(strlen(tmpBuf) * sizeof(char) + 1);
  if ( ! theLine){
    fprintf (stderr, "%s: Error allocating memory in parseCookieFile().\n", progName);
    return ztMemoryAllocate;
  }
  strcpy(theLine, tmpBuf);

  /* is this a cookie file? **/
  string = strstr(theLine, fileMarker);
  if( ! string || (string != theLine)){
    fprintf (stderr, "%s: Error not Geofabrik cookie file in parseCookie(), failed marker test.\n", progName);
    return ztNotCookieFile;
  }


  loginToken = strtok (theLine, semicol);
  if ( ! loginToken){

    fprintf(stderr, "%s: Error parseCookieFile() failed to extract loginToken!\n", progName);
    return ztParseError;
  }

  expireToken = strtok (NULL, semicol);
  if ( ! expireToken){

    fprintf(stderr, "%s: Error parseCookieFile() failed to extract expireToken!\n", progName);
    return ztParseError;
  }

  acceptProto = strtok (NULL, semicol);
  if (! acceptProto ){

    fprintf(stderr, "%s: Error parseCookieFile() failed to extract acceptProto token.\n", progName);
    return ztParseError;
  }
  removeSpaces(&acceptProto);

  pathToken = strtok (NULL, semicol);
  if ( ! pathToken){

    fprintf(stderr, "%s: Error parseCookieFile() failed to extract pathToken!\n", progName);
    return ztParseError;
  }
  removeSpaces(&pathToken);

  sFlagToken = strtok (NULL, semicol);
  if ( ! sFlagToken){

    fprintf(stderr, "%s: Error parseCookieFile() failed to extract sFlagToken!\n", progName);
    return ztParseError;
  }
  removeSpaces(&sFlagToken);

  timeStr = strchr(expireToken, '=');
  if ( ! timeStr){

    fprintf(stderr, "%s: Error parseCookieFile() failed to extract timeStr!\n", progName);
    return ztParseError;
  }
  timeStr++; /* move after the '=' character **/

  /* set members in cookie structure **/
  dstCookie->token = STRDUP(loginToken);
  dstCookie->expireTimeStr = STRDUP(timeStr);

  /* call parseTimeStr() which sets other members in cookie **/
  result = parseTimeStr (dstCookie, timeStr);
  if (result != ztSuccess){
    fprintf(stderr, "%s: Error parseCookie() failed call to parseTimeStr()\n", progName);
    return result;
  }

  return ztSuccess;
}

/* parseTimeStr(): parses time string in cookie file.
 * cookie file part: expires=Sat, 15 Apr 2023 21:59:29 GMT;
 * Parameter 'str' is set at the letter 'S' in Sat just after '=' sign.
 * Function extracts tokens from left to right, converts numbered strings
 * then fills - sets - COOKIE members.
 *
 */

int parseTimeStr (COOKIE *cookie, char const *str){

  char  *mystr;
  char  *space = "\040\t"; /* space set: [space & tab] **/
  char  *colon = ":";

  char  *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "abcdefghijklmnopqrstuvwxyz"
                   "0123456789,:\040";

  /* not all alphabets - upper & lower cases - are needed. LAZY **/

  char  *dwTkn, // day of week
        *dmTkn, // day of month
        *monTkn,
        *yearTkn,
        *hrTkn,
        *minTkn,
        *secTkn;

  int dayWeekDigit; /* day of week as number [0, 6] 0=Sun **/
  int dayMonthDigit;
  int monthDigit;
  int yearDigit;
  int hrDigit;
  int minDigit;
  int secDigit;

  char *endPtr;

  ASSERTARGS (cookie && str);

  /* get our own copy of 'str' parameter - macro terminates program on failure. **/
  mystr = STRDUP(str);

  /* check for allowed characters **/
  if(strspn(mystr, allowed) != strlen(mystr)){
	fprintf(stderr, "%s: Error parseTimeStr() parameter 'str' has "
			"disallowed character.\n", progName);
    return ztDisallowedChar;
  }

  /* move from left to right to extract tokens from string
   * delimiter is changed for tokens. **/

  dwTkn = strtok (mystr, ",");
  if ( ! dwTkn ){
    fprintf(stderr, "%s: Error parseTimeStr() failed to extract wdTkn!\n", progName);
    return ztParseError;
  }

  /* convert abbreviated day name to number [0-6] --> [Sun=0 -Sat=6]**/
  dayWeekDigit = day2num(dwTkn);
  if (dayWeekDigit == -1){
    fprintf(stderr, "%s: Error failed day2num() function.\n", progName);
    return ztInvalidArg;
  }

  dmTkn = strtok (NULL, space);
  if ( ! dmTkn ){
    fprintf(stderr, "%s: Error parseTimeStr() failed to extract dmTkn!\n", progName);
    return ztParseError;
  }

  /* convert day of month string long - integer **/
  dayMonthDigit = (int) strtol(dmTkn, &endPtr, 10);
  if ( *endPtr != '\0'){

    fprintf(stderr, "%s: Error converting 'dmTkn' string to integer in parseTimeStr().\n"
	    " Failed string: %s\n", progName, dmTkn);
    return ztInvalidArg;
  }

  monTkn = strtok (NULL, space);
  if ( ! monTkn ){
    fprintf(stderr, "%s: Error parseTimeStr() failed to extract mmTkn!\n", progName);
    return ztParseError;
  }

  /* convert abbreviated month string to integer [0-11] --> [0=Jan - 11=Dec] **/
  monthDigit = month2num(monTkn);
  if (monthDigit == -1){
    fprintf(stderr, "%s: Error failed month2num() function.\n"
	    "Failed string: %s\n", progName, monTkn);
    return ztInvalidArg;
  }

  yearTkn = strtok (NULL, space);
  if ( ! yearTkn ){
    fprintf(stderr, "%s: Error parseTimeStr() failed to extract yearTkn!\n", progName);
    return ztParseError;
  }

  yearDigit = (int) strtol(yearTkn, &endPtr, 10);
  if ( *endPtr != '\0'){

    fprintf(stderr, "%s: Error converting 'yearTkn' string to integer in parseTimeStr().\n"
	    " Failed string: %s\n", progName, yearTkn);
    return ztInvalidArg;
  }

  hrTkn = strtok (NULL, colon);
  if ( ! hrTkn ){
    fprintf(stderr, "%s: Error parseTimeStr() failed to extract hrTkn!\n", progName);
    return ztParseError;
  }

  hrDigit = (int) strtol(hrTkn, &endPtr, 10);
  if ( *endPtr != '\0'){

    fprintf(stderr, "%s: Error converting 'hrTkn' string to integer in parseTimeStr().\n"
	    " Failed string: %s\n", progName, hrTkn);
    return ztInvalidArg;
  }

  minTkn = strtok (NULL, colon);
  if ( ! minTkn ){
    fprintf(stderr, "%s: Error parseTimeStr() failed to extract minTkn!\n", progName);
    return ztParseError;
  }

  minDigit = (int) strtol(minTkn, &endPtr, 10);
  if ( *endPtr != '\0'){

    fprintf(stderr, "%s: Error converting 'minTkn' string to integer in parseTimeStr().\n"
	    " Failed string: %s\n", progName, minTkn);
    return ztInvalidArg;
  }

  secTkn = strtok (NULL, space);
  if ( ! secTkn ){
    fprintf(stderr, "%s: Error parseTimeStr() failed to extract secTkn!\n", progName);
    return ztParseError;
  }

  secDigit = (int) strtol(secTkn, &endPtr, 10);
  if ( *endPtr != '\0'){

    fprintf(stderr, "%s: Error converting 'secTkn' string to integer in parseTimeStr().\n"
	    " Failed string: %s\n", progName, secTkn);
    return ztInvalidArg;
  }

  /* fill 'tm struct' members
   * make sure to fill required members to get accurate conversion,
   * write down a date to ensure that, not easy to auto-check! zero is allowed.
   * example date Jun. 02/1958 at: 19:45:17 another 06/02/1958 02:03:47.
   * Required members are six:
   *  - year
   *  - month
   *  - day of month
   *  - hour
   *  - minute
   *  - second
   *
   ***************************************************************************/

  cookie->expireTM.tm_year = yearDigit - 1900;

  cookie->expireTM.tm_mon = monthDigit;

  cookie->expireTM.tm_mday = dayMonthDigit;

  cookie->expireTM.tm_wday = dayWeekDigit; /* mktime() ignores tm_wday & tm_yday members **/

  cookie->expireTM.tm_hour = hrDigit;

  cookie->expireTM.tm_min = minDigit;

  cookie->expireTM.tm_sec = secDigit;

  /* fill expireSeconds member by converting filled 'expireTM' structure to
   * time_t time value in seconds **/
  cookie->expireSeconds = makeTimeGMT(&(cookie->expireTM));

  if(cookie->expireSeconds == -1) /* check return from makeTimeGMT() **/

	return ztParseError;

  return ztSuccess;

} /* END parseTimeStr() **/


/* pipeSpawnScript(): runs script in a pipe AND gets script STDERR output
 * in char **outputString variable
 *
 * How oauth_cookie_client.py script work:
 *   - script usually outputs the cookie string to terminal on success: standard output.
 *   - on failure script writes an error message with server response code to
 *     "STANDARD ERROR" then exits.
 *   - cookie string can be written to a file with -o (--output) option which is used
 *     in argList here, so on success there is no terminal output
 *   - this function gets the error message in its "outputString" variable from
 *     STDERR_FILENO. Again "STANDARD ERROR" not "standard output".
 *
 * Returns: ztFailedSysCall, ztMemoryAllocate, ztChildProcessFailed, ztSuccess.
 * In case of "ztChildProcessFailed" script error message is copied into the
 * third variable (outputString). Parameter (outputString) is set to NULL if
 * we can not read error message.
 *
 ******************************************************************************/

int pipeSpawnScript (const char *prog, char* const argList[], char **outputString){

  /* function prototype to match execv() system call from man page:
   *
   *      int execv(const char *pathname, char *const argv[]);
   *
   ******************************************************************/

  pid_t childPid;
  int	fds[2];          /* pipe fds are in alphabetical order (0->read & 1->write) */
  FILE	*scriptTerminal; /* so we can use fgets() */
  int	waitStatus;      /* exit status; code */
  int	result;
  char	temBuf[1024] = {0};

  errno = 0;

  // create the pipe
  result = pipe(fds);

  if (result == -1){ // failed pipe() call

    perror ("pipe");
    fprintf (stderr, "%s: Error failed system call to pipe()\n", progName);
    // TODO: get errno and provide better error message
    return ztFailedSysCall;
  }

  childPid = fork();
  if (childPid == -1){ // failed fork() call

    perror ("fork");
    fprintf (stderr, "%s: Error failed system call to fork()\n", progName);
    // TODO: get errno and provide better error message
    return ztFailedSysCall;
  }

  if (childPid == (pid_t) 0){ // this is the child, do child work

    // close the pipe read end
    close (fds[0]);

    scriptTerminal = fdopen (fds[1], "w");

    /* connect pipe write end to STANDARD ERROR:
     *   this is like shell redirection.
     * Note that we ignore standard output.
     * *********************************************************************/
    result = dup2 (fds[1], STDERR_FILENO);
    if (result == -1){

      perror ("dup2");
      fprintf (stderr, "%s: Error failed system call to dup2()\n", progName);
      return ztFailedSysCall;
    }

    // run the script; this replaces the child process!
    execv (prog, argList);

    /* The execv function returns only if an error occurs. */
    fprintf (stderr, "%s: Error: I am the CHILD in pipeSpawnScript():\n"
	     "If you see this then there was an error in execv() call...\n", progName);
    fprintf (stderr, "%s: Error in pipeSpawnScript(): an error occurred in"
	     " execv() ... aborting!\n",
	     progName);
    abort();  // TODO: return error AND remove abort()

    fflush(scriptTerminal);

    close (fds[1]);

  } // end child work

  else { // this is the parent; do parent work

    // close the pipe write end
    close (fds[1]);

    // wait for the child to finish AND store its exit status
    waitpid (childPid, &waitStatus, 0);

    if (WEXITSTATUS(waitStatus) == EXIT_SUCCESS) {

      return ztSuccess;
    }

    else { /*  WEXITSTATUS(waitStatus) != EXIT_SUCCESS
	    * failed script -> get its error text message */

      // convert script output terminal to FILE * ,,, so we can use fgets() function
      scriptTerminal = fdopen (fds[0], "r");

      if ( ! scriptTerminal ){

	perror ("fdopen");
	fprintf (stderr, "%s: Error returned from fdopen() call.\n", progName);
	return ztFailedSysCall;
      }

      while ( !feof (scriptTerminal)) // script writes only ONE line, still use while()

	fgets (temBuf, 1023, scriptTerminal);

      /* maybe there is nothing to get or read! */
      if (strlen(temBuf) == 0){

	*outputString = NULL; /* make sure it is NULL in this case */
	return ztChildProcessFailed;
      }

      // remove linefeed character
      temBuf[strlen(temBuf) -1] = '\0';

      *outputString = (char *) malloc ((strlen(temBuf) + 1) * sizeof(char));
      if ( *outputString == NULL){

	fprintf(stderr, "%s: Error allocating memory in pipeSpawnScript().\n", progName);
	return ztMemoryAllocate;
      }

      strcpy (*outputString, temBuf);

      fprintf (stderr, "pipeSpawnScript(): The script failed to retrieve cookie; error message from script was:\n  < %s >\n\n", temBuf);

      return ztChildProcessFailed;

    } // end failed script

  } // end parent work

} /* END pipeSpawnScript() **/

/* getResponseCode(): Function extracts 'response code' ABC from the following
 *  exact string message: received HTTP code ABC but expected 200
 *  function sets the integer pointed to by code to response code on ztSuccess.
 *  On errors code is set to zero except when error is ztHighResponse.
 *
 * NOTE: edited script at original line # 67 by removing the word "status" from
 * error message:
 * report_error("POST {}, received HTTP status code {} but expected 200".format(url, r.status_code))
 * became:
 * report_error("POST {}, received HTTP code {} but expected 200".format(url, r.status_code))
 * this way ALL error messages now start with the phrase: "received HTTP code".
 * also added a comment line below as:
 * # NOTE: removed the word 'status' from the above line. W.H.\n
 *
 *  returns:
 *  ztInvalidArg : message does not match "received HTTP code ABC but expected 200"
 *  ztMemoryAllocate : could not allocate memory for own message copy
 *  ztEmptyString : Empty error message (msg)
 *  ztBadResponse : failed to convert string to long - has non digit maybe
 *  ztHighResponse : when code > 599
 *  ztSuccess.
 *
 *  TODO: rename function to getScriptResponseCode()
 *
 ******************************************************************************/

int getResponseCode (int *code, char *msg){

  char  *myMsg;
  char *startDigit = "123456789";
  char *startMarker = "received HTTP code";
  char *subStr1;
  char *expectedStr = "but expected 200";
  char *subStr2;
  char *codeStr;
  char *codeToken;
  char *spaceDel = "\040";
  int codeNum;
  char *endPtr;


  ASSERTARGS (code && msg);

  *code = 0; /* only change when we get it */

  if (strlen(msg) == 0){

    fprintf (stderr, "%s: Error in getResponseCode(): Empty string in msg parameter.\n",
	     progName);
    return ztEmptyString;
  }

  /* msg must include startMarker AND expectedStr strings */
  subStr1 = strstr (msg, startMarker);

  if ( ! subStr1 || (subStr1 != msg)){

    fprintf (stderr, "%s: Error in getResponseCode(): invalid msg parameter: <%s>\n",
	     progName, msg);
    return ztInvalidArg;
  }

  subStr2 = strstr (msg, expectedStr);

  if ( ! subStr2 || (strcmp(subStr2, expectedStr) != 0)){

    fprintf (stderr, "%s: Error in getResponseCode(): invalid msg parameter: <%s>\n",
	     progName, msg);
    return ztInvalidArg;
  }

  myMsg = strdup(msg);

  if ( ! myMsg ){

    fprintf (stderr, "%s: Error in getResponseCode(): memory allocation.\n", progName);
    return ztMemoryAllocate;
  }

  /* find the start of received code string */
  codeStr = strpbrk(myMsg, startDigit);

  if ( ! codeStr ){

    fprintf (stderr, "%s: Error in getResponseCode(): invalid msg parameter: <%s>\n",
	     progName, msg);
    return ztInvalidArg;
  }

  codeToken = strtok(codeStr, spaceDel);

  codeNum = (int) strtol (codeToken, &endPtr, 10);

  if ( *endPtr != '\0' ){ /* may have something other than digits */

    fprintf (stderr, "%s: Error in getResponseCode(): invalid msg parameter: <%s>\n",
	     progName, msg);
    return ztParseError; //ztBadResponse;
  }

  *code = codeNum;

  if (codeNum > 599)

    return ztResponseUnknown; //ztHighResponse;

  return ztSuccess;
}

void logUnseen(SETTINGS *settings, char *msg, char *lastPart) {

  char  *unseenName = "UNSEEN_RESPONSE.txt";
  FILE  *unseenFilePtr;
  char  *myTime;
  pid_t  myPid;
  char  *text;
  char  tmpBuf[PATH_MAX];

  ASSERTARGS(settings);

  errno = 0;

  sprintf(tmpBuf, "%s/%s", settings->workDir, unseenName);
  unseenFilePtr = fopen(tmpBuf, "a");
  if (unseenFilePtr == NULL) {
    fprintf(stderr,
	    "%s: Error could not open UNSEEN_RESPONSE log file! <%s>\n",
	    progName, tmpBuf);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    // return ztOpenFileError;
  } else { /* we have opened file */

    myTime = formatC_Time();
    fprintf(unseenFilePtr, "%s: UNSEEN ERROR started at: %s\n", progName,
	    myTime);

    myPid = getpid();
    myTime = formatMsgHeadTime();

    if (msg) {

      text = "Received error message from script: ";
      fprintf(unseenFilePtr, "%s [%d] %s\n", myTime, (int) myPid, text);
      fprintf(unseenFilePtr, "   <%s>\n\n", msg);

      if (lastPart) {

	text = "lastPart was: ";
	fprintf(unseenFilePtr, "%s [%d] %s\n", myTime, (int) myPid,
		text);
	fprintf(unseenFilePtr, "   <%s>\n\n", lastPart);
      } else { /* no lastPart */

	text = "Could NOT get lastPart from error message text.";
	fprintf(unseenFilePtr, "%s [%d] %s\n", myTime, (int) myPid,
		text);
      }
    } else { /* empty msg */

      text = "EMPTY error message from script! EMPTY. ";
      fprintf(unseenFilePtr, "%s [%d] %s\n", myTime, (int) myPid, text);
    }

    text = "Current program settings below:";
    fprintf(unseenFilePtr, "%s [%d] %s\n", myTime, (int) myPid, text);

    printSettings(unseenFilePtr, settings);

    fprintf(unseenFilePtr, "++++++++++++++ Done Unseen ++++++++++++\n\n");

    fflush(unseenFilePtr);
    fclose(unseenFilePtr);
  }

  return;
}

/* getCookieRetry ():
 *
 * short wait  --> sleep 10 seconds
 * normal wait --> sleep 5 minutes
 *
 * Returns:
 *  ztSuccess,
 *  ztPyExecNotFound,
 *  ztOutResource,
 *  ztUnknownError,
 *  ztUnrecognizedMsg,
 *  ztHighResponse,
 *  ztResponse403,
 *  ztResponse429,
 *  ztResponse500,
 *
 *
 */

int getCookieRetry (SETTINGS *settings){

  int 	result;
  int   delayTime = 1 * 30; /* time in seconds **/

  ASSERTARGS(settings && settings->usr && settings->pswd);

  result = getCookieFile(settings);
  if(result == ztSuccess)

    return result;

  /* failed getCookieFile() function - check global serverResponse **/
  if( ! serverResponse ){

    fprintf(stderr, "%s: Error failed getCookieFile() function without Server Response Code!\n"
	    "Function failed for: <%s>, giving up.\n", progName, ztCode2Msg(result));
    return result;
  }
  else if (serverResponse == 500){

    fprintf(stderr, "%s: Error failed getCookieFile() function with Server Response Code 500.\n"
	    "Retrying in <%d> seconds.\n", progName, delayTime);

    sleep(delayTime);

    result = getCookieFile(settings);
  }
  else{

    fprintf(stderr, "%s: Error failed getCookieFile() function. Server Response Code was not 500.\n"
	    "Function failed for: <%s>. Retry is only for response 500.\n",
	    progName, ztCode2Msg(result));
    return result;
  }

  return result;

} /* END getCookieRetry() */

int initialCookie(SETTINGS *settings){

  int   result;

  ASSERTARGS(settings && settings->usr && settings->pswd && settings->cookieFile);

  if( ! cookie ){

    cookie = (COOKIE *)malloc(sizeof(COOKIE));
    if(! cookie){
      fprintf(stderr, "%s: Error allocating memory in initialCookie().\n", progName);
      return ztMemoryAllocate;
    }
    memset(cookie, 0, sizeof(COOKIE));
  }

  result = isFileUsable(settings->cookieFile);
  if(result != ztSuccess){

    result = getCookieRetry(settings);
    if(result != ztSuccess){

      fprintf(stderr, "%s: Error failed getCookieRetry() function.\n", progName);
      return result;
    }
  }

  int notCookieFileFlag = 0;
  int expiredFlag = 0;

  result = parseCookieFile(cookie, settings->cookieFile);

  /* parseCookieFile() may return ztNotCookieFile -
   * we retrieve a new file in this case **/
  notCookieFileFlag = (result == ztNotCookieFile);


  if(result == ztSuccess){

    /* expiredFlag is set only when parseCookieFile() returns ztSuccess **/
    expiredFlag = isExpiredCookie(cookie) == TRUE;
    if(!expiredFlag)

      return ztSuccess;
  }

  if(notCookieFileFlag || expiredFlag){

    result = getCookieRetry(settings);
    if(result != ztSuccess){

      fprintf(stderr, "%s: Error failed getCookieRetry() function.\n", progName);
      return result;
    }
  }

  result = parseCookieFile(cookie, settings->cookieFile);
  if(result != ztSuccess){

    fprintf(stderr, "%s: Error failed parseCookieFile() function.\n", progName);
    return result;
  }

  return ztSuccess;

} /* END initialCookie() **/

char *getCookieToken(){

  static char  *value2Return;

  if(! (cookie && cookie->token) ){
    fprintf(stderr, "getCookieToken(): Error; COOKIE structure is not initialized."
	    " Call initialCookie() first.\n");
    return NULL;
  }

  value2Return = STRDUP(cookie->token);

  return value2Return;

} /* END getCookieToken() **/

void destroyCookie(){

  if(cookie){

    memset(cookie, 0, sizeof(COOKIE));
    free(cookie);
    cookie = NULL;
  }

  return;

} /* END destroyCookie() **/

int isGoodCookieFile(char *name){

  /* TODO: write function **/

  return ztSuccess;
}

/* makeTimeGMT():
 *
 * inverts 'gmtime' turning an input struct tm into a 'time_t' value.
 * mktime() version for GMT time filled structure 'tm'.
 * function returns time value regardless of time zone; GMT in function
 * name does NOT mean input is GMT time.
 *
 * *) there is no mkgmtime()
 * *) man page for timegm() says to avoid using it!
 * *) mktime() is standardized
 *
 * Source: http://www.catb.org/esr/time-programming/
 *
 * Check ranges for all REQUIRED values in the structure;
 * Required values: year, month, day of month, hour, minute and second.
 * Year range checked is between [1964-2064].
 *
 * Return:
 * Time value for filled structure OR on error returns the value (time_t) -1.
 *
 *****************************************************************************/

time_t makeTimeGMT(struct tm *tm){

  time_t ret;
  char *tz;

  time_t invalidValue = -1;

  ASSERTARGS(tm);

/*
  printf("tm->tm_year: <%d>\n"
		  "tm->tm_mon: <%d>\n"
		  "tm->tm_mday: <%d>\n"
		  "tm->tm_hour: <%d>\n"
		  "tm->tm_min: <%d>\n"
		  "tm->tm_sec: <%d>\n",
		  tm->tm_year, tm->tm_mon, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);
*/
  /* check members values:
   * year, month, day of month, hour, minute and seconds. **/

  /* year range [1964-2064]
   * better way is something around current year!
   * Or current century .... **/
  if(tm->tm_year < (1964 -1900) ||
     tm->tm_year > (2064 - 1900))

	return invalidValue;

  if(tm->tm_mon < 0 || tm->tm_mon > 11)

	return invalidValue;

  if(tm->tm_mday < 1 || tm->tm_mday > 31)

	return invalidValue;

  if(tm->tm_hour < 0 || tm->tm_hour > 23)

	return invalidValue;

  if(tm->tm_min < 0 || tm->tm_min > 59)

	return invalidValue;

  /* second can be 60 to account for leap second **/
  if(tm->tm_sec < 0 || tm->tm_sec > 60)

	return invalidValue;

  errno = 0;

  tz = getenv("TZ");

  if (tz)

    tz = STRDUP(tz);

  setenv("TZ", "", 1);

  tzset();

  /* no day light saving. function fails WITHOUT this sometimes! **/
  tm->tm_isdst = 0;

  ret = mktime(tm);

  if(ret == -1){
	perror("The call to mktime() failed!");
	fprintf(stderr, "System error message: %s\n\n", strerror(errno));
  }

  if (tz) {

    setenv("TZ", tz, 1);
    free(tz);
  }
  else

    unsetenv("TZ");

  tzset();

  return ret;

} /* END makeTimeGMT() **/
