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

#define LOG_UNSEEN

int serverResponse = 0;

int writeScript (char	 *fileName){

    /* we depend on this script, I just could not risk leaving it in its own file */

    FILE	*filePtr;
    int	result;

    ASSERTARGS(fileName);

    errno = 0;
    filePtr = fopen ( fileName, "w");
    if ( filePtr == NULL){
        fprintf (stderr, "%s: Error could not create file! <%s>\n",
                progName, fileName);
        printf("System error message: %s\n\n", strerror(errno));
        return ztCreateFileErr;
    }

char const *script =
"#! /usr/bin/env python3\n"
"\n"
"import argparse\n"
"import json\n"
"import re\n"
"import requests\n"
"import sys\n"
"from getpass import getpass\n"
"\n"
"CUSTOM_HEADER = {\"user-agent\": \"oauth_cookie_client.py\"}\n"
"\n"
"def report_error(message):\n"
"    sys.stderr.write(\"{}\\n\".format(message))\n"
"    exit(1)\n"
"\n"
"\n"
"def find_authenticity_token(response):\n"
"    \"\"\"\n"
"    Search the authenticity_token in the response of the server\n"
"    \"\"\"\n"
"    pattern = r\"name=\\\"csrf-token\\\" content=\\\"([^\\\"]+)\\\"\"\n"
"    m = re.search(pattern, response)\n"
"    if m is None:\n"
"        report_error(\"Could not find the authenticity_token in the website to be scraped.\")\n"
"    try:\n"
"        return m.group(1)\n"
"    except IndexError:\n"
"        sys.stderr.write(\"ERROR: The login form does not contain an authenticity_token.\\n\")\n"
"        exit(1)\n"
"\n"
"\n"
"parser = argparse.ArgumentParser(description=\"Get a cookie to access service protected by OpenStreetMap OAuth 1.0a and osm-internal-oauth\")\n"
"parser.add_argument(\"-o\", \"--output\", default=None, help=\"write the cookie to the specified file instead to STDOUT\", type=argparse.FileType(\"w+\"))\n"
"parser.add_argument(\"-u\", \"--user\", default=None, help=\"user name\", type=str)\n"
"parser.add_argument(\"-p\", \"--password\", default=None, help=\"Password, leave empty to force input from STDIN.\", type=str)\n"
"parser.add_argument(\"-s\", \"--settings\", default=None, help=\"JSON file containing parameters\", type=argparse.FileType(\"r\"))\n"
"parser.add_argument(\"-c\", \"--consumer-url\", default=None, help=\"URL of the OAuth cookie generation API of the provider who provides you OAuth protected access to their ressources\", type=str)\n"
"parser.add_argument(\"-f\", \"--format\", default=\"http\", help=\"Output format: 'http' for the value of the HTTP 'Cookie' header or 'netscape' for a Netscape-like cookie jar file\", type=str, choices=[\"http\", \"netscape\"])\n"
"parser.add_argument(\"--osm-host\", default=\"https://www.openstreetmap.org/\", help=\"hostname of the OSM API/website to use (e.g. 'www.openstreetmap.org' or 'master.apis.dev.openstreetmap.org')\", type=str)\n"
"\n"
"\n"
"args = parser.parse_args()\n"
"settings = {}\n"
"if args.settings is not None:\n"
"    settings = json.load(args.settings)\n"
"\n"
"username = settings.get(\"user\", args.user)\n"
"if username is None:\n"
"    username = input(\"Please enter your user name and press ENTER: \")\n"
"if username is None:\n"
"    report_error(\"The username must not be empty.\")\n"
"password = settings.get(\"password\", args.password)\n"
"if password is None:\n"
"    password = getpass(\"Please enter your password and press ENTER: \")\n"
"if len(password) == 0:\n"
"    report_error(\"The password must not be empty.\")\n"
"\n"
"osm_host = settings.get(\"osm_host\", args.osm_host)\n"
"consumer_url = settings.get(\"consumer_url\", args.consumer_url)\n"
"if consumer_url is None:\n"
"    report_error(\"No consumer URL provided\")\n"
"\n"
"# get request token\n"
"url = consumer_url + \"?action=request_token\"\n"
"r = requests.post(url, data={}, headers=CUSTOM_HEADER)\n"
"if r.status_code != 200:\n"
"    report_error(\"POST {}, received HTTP status code {} but expected 200\".format(url, r.status_code))\n"
"json_response = json.loads(r.text)\n"
"authorize_url = osm_host + \"/oauth/authorize\"\n"
"try:\n"
"    oauth_token = json_response[\"oauth_token\"]\n"
"    oauth_token_secret_encr = json_response[\"oauth_token_secret_encr\"]\n"
"except KeyError:\n"
"    report_error(\"oauth_token was not found in the first response by the consumer\")\n"
"\n"
"# get OSM session\n"
"login_url = osm_host + \"/login?cookie_test=true\"\n"
"s = requests.Session()\n"
"r = s.get(login_url, headers=CUSTOM_HEADER)\n"
"if r.status_code != 200:\n"
"    report_error(\"GET {}, received HTTP code {}\".format(login_url, r.status_code))\n"
"\n"
"# login\n"
"authenticity_token = find_authenticity_token(r.text)\n"
"login_url = osm_host + \"/login\"\n"
"r = s.post(login_url, data={\"username\": username, \"password\": password, \"referer\": \"/\", \"commit\": \"Login\", \"authenticity_token\": authenticity_token}, allow_redirects=False, headers=CUSTOM_HEADER)\n"
"if r.status_code != 302:\n"
"    report_error(\"POST {}, received HTTP code {} but expected 302\".format(login_url, r.status_code))\n"
"\n"
"# authorize\n"
"authorize_url = \"{}/oauth/authorize?oauth_token={}\".format(osm_host, oauth_token)\n"
"r = s.get(authorize_url, headers=CUSTOM_HEADER)\n"
"if r.status_code != 200:\n"
"    report_error(\"GET {}, received HTTP code {} but expected 200\".format(authorize_url, r.status_code))\n"
"authenticity_token = find_authenticity_token(r.text)\n"
"\n"
"post_data = {\"oauth_token\": oauth_token, \"oauth_callback\": \"\", \"authenticity_token\": authenticity_token, \"allow_read_prefs\": [0, 1], \"commit\": \"Save changes\"}\n"
"authorize_url = \"{}/oauth/authorize\".format(osm_host)\n"
"r = s.post(authorize_url, data=post_data, headers=CUSTOM_HEADER)\n"
"if r.status_code != 200:\n"
"    report_error(\"POST {}, received HTTP code {} but expected 200\".format(authorize_url, r.status_code))\n"
"\n"
"# logout\n"
"logout_url = \"{}/logout\".format(osm_host)\n"
"r = s.get(logout_url, headers=CUSTOM_HEADER)\n"
"if r.status_code != 200 and r.status_code != 302:\n"
"    report_error(\"POST {}, received HTTP code {} but expected 200 or 302\".format(logout_url))\n"
"\n"
"# get final cookie\n"
"url = consumer_url + \"?action=get_access_token_cookie&format={}\".format(args.format)\n"
"r = requests.post(url, data={\"oauth_token\": oauth_token, \"oauth_token_secret_encr\": oauth_token_secret_encr}, headers=CUSTOM_HEADER)\n"
"\n"
"cookie_text = r.text\n"
"if not cookie_text.endswith(\"\\n\"):\n"
"    cookie_text += \"\\n\"\n"
"\n"
"if not args.output:\n"
"    sys.stdout.write(cookie_text)\n"
"else:\n"
"    args.output.write(cookie_text)\n";

    result = fprintf (filePtr, "%s", script);
    if (result != strlen(script)){

        if (result < 0){
            fprintf (stderr, "%s: Output error in fprintf() call in writeScript() function.\n", progName);
            return ztWriteError;
        }

        fprintf (stderr, "%s: writeScript(): Error returned from fprintf() to script file\n"
                "result does not match script length!\n", progName);
        return ztWriteError;
    }

    fflush(filePtr);
    fclose(filePtr);

    return ztSuccess;

}

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
 **********************************************************************************/

int getCookieFile (SETTINGS *settings){

    char    *pyExec = "/usr/bin/python3";
    char    *argsList[5];
    char    tmpBuf[PATH_MAX];
    int       result;
    char    *msg;
    char    *markerStr = "received HTTP code"; /* included in script error message */
    char    *lastPart = NULL;
    int       responseCode = 0; /* only change when we get from getResponseCode() */
    int       returnValue = 0;
    int       pssResult; /* returned result from pipeSpawnScript() */

    ASSERTARGS (settings && settings->scriptFile && settings->jsonFile);

    if( IsArgUsableFile(pyExec) != ztSuccess ){
        fprintf(stderr, "%s: Error could not find file: <%s>.\n", progName, pyExec);
        return ztPyExecNotFound;
    }

    result = writeScript(settings->scriptFile);
    if (result != ztSuccess){
        fprintf(stderr, "%s: Error failed to write script to file.\n", progName);
        return result;
    }

    result = writeJSONfile(settings->jsonFile, settings);
    if (result != ztSuccess){
        fprintf(stderr, "%s: Error failed to write JSON settings file.\n", progName);
        remove(settings->scriptFile);
        return result;
    }

    argsList[0] = pyExec;
    argsList[1] = settings->scriptFile;

    sprintf (tmpBuf, "-s%s", settings->jsonFile);
    argsList[2] = strdup(tmpBuf);

    sprintf (tmpBuf, "-o%s", settings->cookieFile);
    argsList[3] = strdup(tmpBuf);

    argsList[4] = NULL;

    pssResult = pipeSpawnScript (pyExec, argsList, &msg);

    if (pssResult == ztSuccess){

        serverResponse = 200; /* no script output to check! */
        /* check cookie file status */
        result = IsArgUsableFile(settings->cookieFile);
        if ( result != ztSuccess )
            returnValue = ztUnusableFile;
    }
    else { /* failed script result */
              /* examine and parse its error message (3rd parameter "msg") */

        if (msg == NULL) {

#ifdef LOG_UNSEEN
    logUnseen (settings, "ztGotNull error: (msg == NULL)", "NULL");
#endif
            fprintf (stderr, "%s: Error failed script with NULL error msg in getCookieFile().\n", progName);
            returnValue = ztGotNull;
        }
        else if (strlen(msg) == 0){

#ifdef LOG_UNSEEN
    logUnseen (settings, "ztEmptyString error: (strlen(msg) == 0)", "NULL");
#endif

            fprintf (stderr, "%s: Error failed script with EMPTY error msg in getCookieFile().\n", progName);
            returnValue = ztEmptyString;
        }
        else if (msg){

            lastPart = strstr(msg, markerStr);
            if ( ! lastPart ){
                /* error message doesn't include markerStr -> unrecognized message
                 * for now we log such error messages */
#ifdef LOG_UNSEEN
    sprintf (tmpBuf, "UNRECOGNIZED msg: <%s>", msg);
    logUnseen (settings, tmpBuf, "No lastPart");
#endif

                fprintf (stderr, "%s: Error failed script with UNRECOGNIZED server error"
                		                 " message in getCookieFile()\n "
                                         "  Server Error Message: <%s>.\n", progName, msg);
                returnValue = ztUnrecognizedMsg;
            }
            else { /* markerStr IS included in message */

                result = getResponseCode (&responseCode, lastPart);
                if (result != ztSuccess){

                	/* handle getResponseCode() errors:
                	 *  ztInvalidArg & ztEmptyString are handled above */

                    if (result == ztMemoryAllocate){
                        returnValue = ztMemoryAllocate;
                    }

                    if (result == ztBadResponse){
#ifdef LOG_UNSEEN
    sprintf (tmpBuf, "ztBadResponse error from msg: <%s>", msg);
    logUnseen (settings, tmpBuf, lastPart);
#endif
                        returnValue = ztBadResponse;
                    }

                    if (result == ztHighResponse){
#ifdef LOG_UNSEEN
    sprintf (tmpBuf, "ztHighResponse from msg: <%s>", msg);
    logUnseen (settings, tmpBuf, lastPart);
#endif
                        serverResponse = responseCode;
                        returnValue = ztHighResponse;
                    }
                } /* end getResponseCode() errors */

                serverResponse = responseCode;
                returnValue = pssResult;
            } /* end else {* markerStr IS included in message * */
        }
    } /* end if script failed */

    result = removeFiles(settings);
    if (result != ztSuccess)
        fprintf(stderr, "%s: Warning failed to remove temporary file(s)!\n", progName);

    if (returnValue != 0)

        return returnValue;

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
        printf("Login Token is: %s\n\n", ck->token);
    else
        printf("Login Token is NOT set.\n");

    if (ck->expYear)
        printf("Expire Year String : <%s> as digits: [%d]\n", ck->expYear, ck->year);
    else
        printf ("Expire year is not set.\n");

    if (ck->expMonth)
        printf("Expire Month String is: <%s> as digits: [%d]\n", ck->expMonth, ck->month);
    else
        printf("Expire Month is not set.\n");

    if (ck->expDayMonth)
        printf("Expire Day of Month String is: <%s> as digits: [%d]\n", ck->expDayMonth, ck->dayMonth);
    else
        printf("Expire Day of Month is not set.\n");

    if (ck->expDayWeek)
        printf("Expire Day of Week String is: <%s> as digits: [%d]\n", ck->expDayWeek, ck->dayWeek);
    else
        printf("Expire Day of Week is not set.\n");

    if (ck->expHour)
        printf("Expire Hour String is: <%s> as digits: [%d]\n", ck->expHour, ck->hour);
    else
        printf("Expire Hour is not set.\n");

    if (ck->expMinute)
        printf("Expire Minute String is: <%s> as digits: [%d]\n", ck->expMinute, ck->minute);
    else
        printf("Expire Minute is not set.\n");

    if (ck->expSecond)
        printf("Expire Second String is: <%s> as digits: [%d]\n", ck->expSecond, ck->second);
    else
        printf("Expire Second is not set.\n");

    if (ck->format)
        printf("Format String is: <%s>\n", ck->format);
    else
        printf("Format string is not set.\n");

    if (ck->path)
        printf("Path String is: <%s>\n", ck->path);
    else
        printf("Path string is not set.\n");

    if (ck->sFlag)
        printf("sFlag String is: <%s>\n", ck->sFlag);
    else
        printf("sFlag string is not set.\n");

    return;
}

int isExpiredCookie(COOKIE *ck){

    /* returns TRUE if cookie expires within the next hour from current time
    * we do not look at the minutes.
    **********************************************************************/
    struct tm	*nowPtr; // we do not allocate memory
    time_t			timeValue;
    char				*formattedTime; // we do not allocate space

    time(&timeValue);

    nowPtr = gmtime(&timeValue);

    formattedTime = asctime(nowPtr);

    fprintf(stdout, "Current GMT time is : %s", formattedTime);
    fprintf(stdout, "Cookie expire time at: %s\n", ck->expireTimeStr);

/*
printf("GMT current time by members: yyyy :: mon :: Date :: hh:mm:ss\n");
printf("GMT current time by member: %d :: %d :: %d :: %d:%d:%d\n",
            nowPtr->tm_year + 1900, nowPtr->tm_mon, nowPtr->tm_mday,
            nowPtr->tm_hour, nowPtr->tm_min, nowPtr->tm_sec);
*/
    if (ck->year < (nowPtr->tm_year + 1900))

        return TRUE;

    if ( (ck->month < nowPtr->tm_mon) &&
        (ck->year == (nowPtr->tm_year + 1900)) )

        return TRUE;

    if ( (ck->dayMonth < nowPtr->tm_mday) &&
        (ck->month == nowPtr->tm_mon) )

        return TRUE;

    if ( (ck->hour < (nowPtr->tm_hour + 2)) &&
        (ck->dayMonth == nowPtr->tm_mday	) )

        return TRUE;

    return FALSE;
}

int parseCookieFile (COOKIE *dstCookie, char	*filename){

    /* example short cookie file -- login-token part was cut short to fit here */
    /* gf_download_oauth="login|2018-04-12|mmPsXFi3-ftGnxdDpv_pI-CVXmmZDi6SU_vNgpuEsl4c0NK_w=="; expires=Wed, 16 Feb 2022 17:51:27 GMT; HttpOnly; Path=/; Secure 	*/

    int		result;
    FILE		*filePtr;
    char		*theLine;
    char		tmpBuf[LONG_LINE] = {'0'};
    int		numLines = 0;
    char		*semicol = ";"; // semicolon and space

    char		*loginToken,
                *expireToken,
                *formatToken,
                *pathToken,
                *sFlagToken;

    char		*timeStr;

    ASSERTARGS (dstCookie && filename);

    result = IsArgUsableFile(filename);
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

    while (fgets(tmpBuf, LONG_LINE - 1, filePtr)){

        /* if we get more than ONE line, we are out of here! */
        numLines++;
    }

    if (numLines > MAX_COOKIE_LINES){

        fprintf (stderr, "%s: Error parseCookieFile() found multiple lines: [%d] in cookie file: <%s>\n",
                progName, numLines, filename);
        fclose(filePtr);
        return ztMissFormatFile;
    }
    /* remove line-feed - fgets() keeps */
    tmpBuf[strlen(tmpBuf) - 1] = '\0';

    theLine = (char *) malloc(strlen(tmpBuf) * sizeof(char) + 1);
    if ( ! theLine){
        fprintf (stderr, "%s: Error allocating memory in parseCookieFile().\n", progName);
        return ztMemoryAllocate;
    }
    strcpy(theLine, tmpBuf);

    fclose(filePtr);

    loginToken = strtok (theLine, semicol);
    if ( ! loginToken){

        fprintf(stderr, "%s: Error parseCookieFile() got null for loginToken!\n", progName);
        return ztGotNull;
    }

    expireToken = strtok (NULL, semicol);
    if ( ! expireToken){

        fprintf(stderr, "%s: Error parseCookieFile() got null for expireToken!\n", progName);
        return ztGotNull;
    }

    formatToken = strtok (NULL, semicol);
    if (! formatToken){

        fprintf(stderr, "%s: Error parseCookieFile() got null for formatToken!\n", progName);
        return ztGotNull;
    }
    removeSpaces(&formatToken);

    pathToken = strtok (NULL, semicol);
    if ( ! pathToken){

        fprintf(stderr, "%s: Error parseCookieFile() got null for pathToken!\n", progName);
        return ztGotNull;
    }
    removeSpaces(&pathToken);

    sFlagToken = strtok (NULL, semicol);
    if ( ! sFlagToken){

        fprintf(stderr, "%s: Error parseCookieFile() got null for sFlagToken!\n", progName);
        return ztGotNull;
    }
    removeSpaces(&sFlagToken);

    timeStr = strchr(expireToken, '=');
    if ( ! timeStr){

        fprintf(stderr, "%s: Error parseCookieFile() got null for initial timeStr!\n", progName);
        return ztGotNull;
    }

    timeStr++;
    if ( ! timeStr){

        fprintf(stderr, "%s: Error parseCookieFile() got null for timeStr!\n", progName);
        return ztGotNull;
    }

    result = parseTimeStr (dstCookie, timeStr);
    if (result != ztSuccess){
        fprintf(stderr, "%s: Error parseCookie() failed call to parseTimeStr()\n", progName);
        return result;
    }

    dstCookie->token = strdup(loginToken);
    dstCookie->format = strdup(formatToken);
    dstCookie->path = strdup(pathToken);
    dstCookie->sFlag = strdup(sFlagToken);

    dstCookie->expireTimeStr = strdup(timeStr);

    return ztSuccess;
}

int parseTimeStr (COOKIE *cookie, char const *str){

    char		*mystr;
    char		*space = "\040\t"; // space or tab
    char		*colon = ":";

    char		*dwTkn, // day of week
                *dmTkn, // day of month
                *monTkn,
                *yearTkn,
                *hrTkn,
                *minTkn,
                *secTkn;

    int 	dayWeekDigit; // day of week
    int	dayMonthDigit;
    int	monthDigit;
    int	yearDigit;
    int	hrDigit;
    int	minDigit;
    int	secDigit;

    char		*endPtr;

    ASSERTARGS (cookie && str);

    mystr = strdup(str);
    if ( ! mystr ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for mystr!\n", progName);
        return ztMemoryAllocate;
    }

    dwTkn = strtok (mystr, ",");
    if ( ! dwTkn ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for wdTkn!\n", progName);
        return ztGotNull;
    }

    dayWeekDigit = day2num(dwTkn);
    if (dayWeekDigit == -1){
        fprintf(stderr, "%s: Error converting abbreviated day to digit! parseTimeStr().\n", progName);
        return ztUnknownError;
    }

    dmTkn = strtok (NULL, space);
    if ( ! dmTkn ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for dmTkn!\n", progName);
        return ztGotNull;
    }
    dayMonthDigit = (int) strtol(dmTkn, &endPtr, 10);
    if ( *endPtr != '\0'){

        fprintf(stderr, "%s: Error converting day of month string to integer in parseTimeStr().\n"
                " Failed string: %s\n", progName, dmTkn);
        return ztUnknownError;
    }

    monTkn = strtok (NULL, space);
    if ( ! monTkn ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for mmTkn!\n", progName);
        return ztGotNull;
    }
    monthDigit = month2num(monTkn);
    if (monthDigit == -1){
        fprintf(stderr, "%s: Error converting abbreviated month to digit! parseTimeStr().\n"
                "Failed string: %s\n", progName, monTkn);
        return ztUnknownError;
    }

    yearTkn = strtok (NULL, space);
    if ( ! yearTkn ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for yyTkn!\n", progName);
        return ztGotNull;
    }
    yearDigit = (int) strtol(yearTkn, &endPtr, 10);
    if ( *endPtr != '\0'){

        fprintf(stderr, "%s: Error converting day of year string to integer in parseTimeStr().\n"
                " Failed string: %s\n", progName, yearTkn);
        return ztUnknownError;
    }

    hrTkn = strtok (NULL, colon);
    if ( ! hrTkn ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for hrTkn!\n", progName);
        return ztGotNull;
    }
    hrDigit = (int) strtol(hrTkn, &endPtr, 10);
    if ( *endPtr != '\0'){

        fprintf(stderr, "%s: Error converting day of hour string to integer in parseTimeStr().\n"
                " Failed string: %s\n", progName, hrTkn);
        return ztUnknownError;
    }

    minTkn = strtok (NULL, colon);
    if ( ! minTkn ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for minTkn!\n", progName);
        return ztGotNull;
    }
    minDigit = (int) strtol(minTkn, &endPtr, 10);
    if ( *endPtr != '\0'){

        fprintf(stderr, "%s: Error converting day of minutes string to integer in parseTimeStr().\n"
                " Failed string: %s\n", progName, minTkn);
        return ztUnknownError;
    }

    secTkn = strtok (NULL, space);
    if ( ! secTkn ){
        fprintf(stderr, "%s: Error parseTimeStr() got null for secTkn!\n", progName);
        return ztGotNull;
    }
    secDigit = (int) strtol(secTkn, &endPtr, 10);
    if ( *endPtr != '\0'){

        fprintf(stderr, "%s: Error converting day of seconds string to integer in parseTimeStr().\n"
                " Failed string: %s\n", progName, secTkn);
        return ztUnknownError;
    }

    cookie->expYear = strdup(yearTkn);
    cookie->year = yearDigit;

    cookie->expMonth = strdup(monTkn);
    cookie->month = monthDigit;

    cookie->expDayMonth = strdup(dmTkn);
    cookie->dayMonth = dayMonthDigit;

    cookie->expDayWeek = strdup(dwTkn);
    cookie->dayWeek = dayWeekDigit;

    cookie->expHour = strdup(hrTkn);
    cookie->hour = hrDigit;

    cookie->expMinute = strdup(minTkn);
    cookie->minute = minDigit;

    cookie->expSecond = strdup(secTkn);
    cookie->second = secDigit;

    return ztSuccess;
}
int writeJSONfile(char *filename, SETTINGS *settings){

    FILE		*filePtr;
    char		*template =
            "{\n"
            "  \"user\": \"%s\",\n"
            "  \"password\": \"%s\",\n"
            "  \"osm_host\": \"https://www.openstreetmap.org\",\n"
            "  \"consumer_url\": \"https://osm-internal.download.geofabrik.de/get_cookie\"\n"
            "}\n";

    ASSERTARGS (filename && settings &&
                        settings->usr && settings->psswd);

    errno = 0;
    filePtr = fopen ( filename, "w");
    if ( filePtr == NULL){
        fprintf (stderr, "%s: Error could not create file! <%s>\n",
                progName, filename);
        printf("System error message: %s\n\n", strerror(errno));
        return ztCreateFileErr;
    }

    fprintf (filePtr, template, settings->usr, settings->psswd);

    fflush (filePtr);

    fclose(filePtr);

    return ztSuccess;
}

int removeFiles (SETTINGS *settings){

    int	result;

    ASSERTARGS(settings);

    /* we only keep cookie file. remove script and json settings files
    * the right way is to use temporary files.
    */
    errno = 0;
    if (settings->scriptFile &&
        (IsArgUsableFile(settings->scriptFile) == ztSuccess)){

        result = remove(settings->scriptFile);
        if (result != 0){
            fprintf (stderr, "%s: Error could remove file! <%s>\n",
                    progName, settings->scriptFile);
            printf("System error message: %s\n\n", strerror(errno));
            return ztFailedSysCall;
        }
    }

    if (settings->jsonFile &&
        (IsArgUsableFile(settings->jsonFile) == ztSuccess)){

        result = remove(settings->jsonFile);
        if (result != 0){
            fprintf (stderr, "%s: Error could remove file! <%s>\n",
                    progName, settings->jsonFile);
            printf("System error message: %s\n\n", strerror(errno));
            return ztFailedSysCall;
        }
    }

    return ztSuccess;
}

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

int pipeSpawnScript (const char *prog, char * const argList[], char **outputString){

	/* function prototype to match execv() system call from man page:
	 *
	 *      int execv(const char *pathname, char *const argv[]);
	 *
	 ******************************************************************/

	pid_t		childPid;
	int		fds[2];                /* pipe fds are in alphabetical order (0->read & 1->write) */
	FILE		*scriptTerminal;   /* so we can use fgets() */
	int		waitStatus;            /* exit status; code */
	int		result;
	char		temBuf[1024] = {0};

	// TODO: use error number! set errno to zero
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
		printf ("%s: Error: I am the CHILD in pipeSpawnScript():\n"
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

}

/* getResponseCode(): Function extracts 'response code' ABC from the following
 *  exact string message: received HTTP code ABC but expected 200
 *  function sets the integer pointed to by code to response code on ztSuccess.
 *  On errors code is set to zero except when error is ztHighResponse.
 *
 *  returns:
 *  ztInvalidArg : message does not match "received HTTP code ABC but expected 200"
 *  ztMemoryAllocate : could not allocate memory for own message copy
 *  ztEmptyString : Empty error message (msg)
 *  ztBadResponse : failed to convert string to long - has non digit maybe
 *  ztHighResponse : when code > 599
 *  ztSuccess.
 *
 ******************************************************************************/

int getResponseCode (int *code, char *msg){

	char		*myMsg;
	char		*startDigit = "123456789";
	char		*startMarker = "received HTTP code";
	char		*subStr1;
	char		*expectedStr = "but expected 200";
	char		*subStr2;
	char		*codeStr;
	char		*codeToken;
	char		*spaceDel = "\040";
	int		codeNum;
	char		*endPtr;


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
		return ztBadResponse;
	}

	*code = codeNum;

	if (codeNum > 599)

		return ztHighResponse;

	return ztSuccess;
}

void logUnseen(SETTINGS *settings, char *msg, char *lastPart) {

	char *unseenName = "UNSEEN_RESPONSE.txt";
	FILE *unseenFilePtr;
	char *myTime;
	pid_t myPid;
	char *text;
	char tmpBuf[PATH_MAX];

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
 * short wait  --> sleep 3 seconds
 * normal wait --> sleep 10 seconds
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

	int	result, retryResult;
	int	receivedCode;

	result = getCookieFile(settings);

	receivedCode = serverResponse;

	switch (result) {

	case ztSuccess:

		return ztSuccess;
		break;

	case ztPyExecNotFound:

		fprintf(stderr, "%s: Error could not find 'python3' executable in default path '/usr/bin/python3'\n"
				" Please see program requirements.\n", progName);
		return ztPyExecNotFound;
		break;

	case ztCreateFileErr:
	case ztWriteError:
	case ztMemoryAllocate:
	case ztFailedSysCall:

		fprintf (stderr, "%s: Error out of resource like memory, disk space or failed system call.\n", progName);
		return ztOutResource;
		break;

	case ztGotNull:
	case ztEmptyString:
	case ztBadResponse:

		/* errors should not happen! maybe lost data? RETRY */
		sleep(3);

		retryResult = getCookieFile(settings);
		if (retryResult == ztSuccess)

			return ztSuccess;

		else {

			fprintf (stderr, "%s: Error unknown error returned from getCookieFile().\n", progName);
			return ztUnknownError;
		}

		break;

	case ztUnrecognizedMsg:

		fprintf (stderr, "%s: Error unrecognized error message received from server.\n", progName);
		return ztUnrecognizedMsg;

		break;

	case ztHighResponse:

		fprintf (stderr, "%s: Error high response code received from server. Response Code > 599.\n", progName);
		return ztHighResponse;

		break;


	case ztChildProcessFailed:

		switch (receivedCode) {

		case 403:

			fprintf(stderr, "%s: Error invalid credentials received from server;\n"
					"wrong user name or password for your OSM account.\n", progName);

			return ztResponse403;
			break;

		case 429:

			fprintf(stderr, "%s: Error \"too many requests error\" received from server.\n"
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

			sleep (10);

			retryResult = getCookieFile(settings);
			if (retryResult == ztSuccess)

				return ztSuccess;

			else {

				fprintf (stderr, "%s: Error failed on retry from 'internal server error'. Please try later.\n", progName);
				if (serverResponse == 500)

					return ztResponse500;

				else

					return ztUnknownError;
			}

			break;

		default:

			fprintf (stderr, "%s: Error script failed to retrieve login cookie with unknown error.\n", progName);
			return ztUnknownError;

			break;

		} /* end switch (receivedCode) */

		break;

	default:

		return ztUnknownError;

		break;

	} /* end switch (result) */

	/* we do not get here! */
	return ztSuccess;
}
