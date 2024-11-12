/*
 * string.c
 *
 *  Created on: Apr 14, 2023
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "ztError.h"
#include "getdiff.h"
#include "usage.h"
#include "util.h"


/* parseCmdLine(): parses command line option using getopt_long().
 * setting on the command line override configuration file setting.
 * parseCmdLine() function fills found settings on the command, could be
 * none, part of or all settings. parseCmdLine() does minimum error checking.
 * Return: ztInvalidArg, calls abort() on memory allocation error.
 *
 * TODO : prependCWD(): file name ALONE --> gets appended to CWD
 * use getcwd() library function.
 ***********************************************************************/

int parseCmdLine(MY_SETTING *arguments, int argc, char* const argv[]){

  int    result;
  static const char *shortOptions = "c:u:p:s:d:b:e:vtnhV";

  static const struct option longOptions[] = {
    {"version", 0, NULL, 'V'},
    {"conf", 1, NULL, 'c'},
    {"user", 1, NULL, 'u'},
    {"passwd", 1, NULL, 'p'},
    {"source", 1, NULL, 's'},
    {"directory", 1, NULL, 'd'},
    {"begin", 1, NULL, 'b'},
	{"end", 1, NULL, 'e'},
    {"verbose", 0, NULL, 'v'},
    {"new", 0, NULL, 'n'},    /* do not require argument **/
    {"text", 0, NULL, 't'},
    {"help", 0, NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int  opt = 0;
  int  longIndex = 0;
  int  confFlag, usrFlag, passwdFlag,
    srcFlag, destFlag, beginFlag, endFlag,
    newFlag; /* do not allow same option twice */

  confFlag = usrFlag = passwdFlag = srcFlag = destFlag = beginFlag = endFlag = newFlag = 0;
  /* This is ugly maybe!
   * It is easy to specify same option more than once with short option?! */

  ASSERTARGS(arguments && argv);
  if(argc == 1)

    /* nothing for us to do! **/
    return ztSuccess;

  do{
    /* fetch one option at a time **/
    opt = getopt_long(argc, argv, shortOptions, longOptions, &longIndex);

    switch(opt){

    case 'h':

      //arguments->help = 1;
      showHelp(); /* this shows help & exits **/
      break;

    case '?':

      fprintf(stderr, "%s: Error '%c' is invalid option.\n", progName, optopt);
      fprintf(stderr, "try: %s -h\n\n", progName);
      return ztInvalidArg;
      break;

    case 'V':

      fprintf(stdout, "%s version: %s\n", progName, VERSION);
      exit(ztSuccess);

    case 'v':

      arguments->verbose = 1;
      break;

    case 'c':

      if (confFlag){
        fprintf(stderr, "%s: Error; duplicate conf option used!\n", progName);
        return ztMalformedCML;
      }

      char  *withPath = NULL;

      /* use arg2FullPath(optarg) instead of this block 1/29/2024 **/

      withPath = arg2FullPath(optarg);

      if(!withPath){
        fprintf(stderr, "%s: Error 'withPath' variable is NULL in parseCmdLine(); Exiting.\n", progName);
        fprintf(stderr, "optarg was: <%s> @@\n\n", optarg);
        fprintf(stderr, " %s: Failed arg2FullPath() function in parseCmdLine().\n", progName);
        return ztUnknownError;
      }

      result = isGoodFilename(withPath);
      if (result != ztSuccess){
        fprintf(stderr, "%s: Error configure filename  <%s> is NOT good filename.\n"
		" Filename is not good for: <%s> \n", progName, withPath, ztCode2Msg(result));
        return result;
      }

      result = isFileUsable(withPath); /* full access to parent directory required **/
      if (result != ztSuccess){
        fprintf(stderr, "%s: Error specified configure file  <%s> is NOT usable file!\n"
		" File is not usable for: <%s> \n", progName, withPath, ztCode2Msg(result));
        return result;
      }

      arguments->configureFile = STRDUP(withPath);

      confFlag = 1;
      break;

    case 'u':

      if (usrFlag){
        fprintf(stderr, "%s: Error; duplicate usr option!\n", progName);
        return ztInvalidArg;
      }

      if (strlen(optarg) > MAX_USER_NAME){
        fprintf(stderr, "%s: Error user name string is too long; maximum allowed length is: <%d> characters.\n"
		" Argument: [%s].\n", progName, MAX_USER_NAME, optarg);
        return ztInvalidArg;
      }
      arguments->usr = STRDUP(optarg);

      usrFlag = 1;
      break;


    case 'p':

      if (passwdFlag){
        fprintf(stderr, "%s: Error; duplicate password option!\n", progName);
        return ztInvalidArg;
      }

      if (strlen(optarg) > MAX_USER_NAME){
        fprintf(stderr, "%s: Error password is too long, maximum length allowed is <%d> characters.\n"
		" Password Length: [%d].\n", progName, MAX_USER_NAME, (int) strlen(optarg));
        return ztInvalidArg;
      }
      arguments->pswd = STRDUP(optarg);

      passwdFlag = 1;
      break;

    case 's':

      if (srcFlag){
	fprintf(stderr, "%s: Error; duplicate source option!\n", progName);
	return ztInvalidArg;
      }

      if (isOkayFormat4URL(optarg) == FALSE) {
	fprintf(stderr, "%s: Error invalid source URL string for source option.\n"
		" Argument: [%s].\n", progName, optarg);
	return ztInvalidArg;
      }
      arguments->source = STRDUP(optarg);

      srcFlag = 1;
      break;

    case 'd':
      /* this is the parent (root) for work directory without "getdiff" entry **/

      if (destFlag){
	fprintf(stderr, "%s: Error; duplicate root for work directory option!\n", progName);
	return ztInvalidArg;
      }

      result = isGoodDirName(optarg);
      if ( result != ztSuccess ){

	fprintf (stderr, "%s: Error specified name <%s> for work directory is not good name.\n"
		 "Directory name is NOT good for: [%s].\n", progName, optarg, ztCode2Msg(result));
	return result;
      }

      result = isDirUsable(optarg);
      if ( result != ztSuccess ){

	fprintf (stderr, "%s: Error specified work directory parent <%s> is not usable directory.\n"
		 "Directory is NOT usable for: [%s].\n", progName, optarg, ztCode2Msg(result));
	return result;
      }

      arguments->rootWD = STRDUP(optarg);

      destFlag = 1;
      break;

    case 'b':

      if (beginFlag){
	fprintf(stderr, "%s: Error; duplicate \"begin\" option!\n", progName);
	return ztInvalidArg;
      }

      if(isGoodSequenceString(optarg) == TRUE){

	arguments->startNumber = STRDUP(optarg);
      }
      else {
	fprintf (stderr, "%s: Error, invalid sequence number for \"begin\" argument.\n"
		 "Valid sequence number is all digits with length between [4 - 9] digits and does not start with '0'.\n"
		 "Invalid argument : [%s].\n", progName, optarg);
	return ztInvalidArg;
      }

      beginFlag = 1;
      break;

    case 'e':

      if (endFlag){
	fprintf(stderr, "%s: Error; duplicate \"end\" option!\n", progName);
	return ztInvalidArg;
      }

      if(isGoodSequenceString(optarg) == TRUE){

	arguments->endNumber = STRDUP(optarg);
      }
      else {
	fprintf (stderr, "%s: Error, invalid sequence number for \"end\" argument.\n"
		 "Valid sequence number is all digits with length between [4 - 9] digits and does not start with '0'.\n"
		 "Invalid argument : [%s].\n", progName, optarg);
	return ztInvalidArg;
      }

      endFlag = 1;
      break;

    case 'n':

      if (newFlag){
	fprintf(stderr, "%s: Error; duplicate \"new\" option!\n", progName);
	return ztInvalidArg;
      }

      arguments->newDifferOff = 1;

      newFlag = 1;
      break;

    case 't':

      arguments->textOnly = 1;
      break;

    case -1: /* done with options */

      break;

    default:
      /* You won't actually get here. See:
	 https://www.ibm.com/developerworks/aix/library/au-unix-getopt.html */
      fprintf(stderr, "%s: getopt_long() in default case! Should not see this! "
	      "Calling abort()\n\n", progName);
      abort();
      break;

    } //end switch(opt)

  } while (opt != -1); /* end do statement **/

  /* there should be no more arguments left **/
  if (optind != argc){
    fprintf(stderr, "%s: Error, malformed command line. Did you forget a dash or inserted space(s) in arguments?\n"
	    "More arguments are left after getopt_long().\n\n", progName);
    /* malformed command line, some reason:
     * single dash with long option,
     * space between dashes with long option,
     * lone single dash.
     *********************************************************************/

    return ztMalformedCML;
  }

  return ztSuccess;

} /* END parseCmdLine() **/

/* parseTimestampLine(): parses time string in timestamp line
 *
 *  timestamp=2023-06-04T20\:22\:01Z
 *
 * timeString parameter is a pointer to the line above.
 *
 * I am aware of strptime() function.
 *
 ************************************************************/
int parseTimestampLine(struct tm *tmStruct, char *timeString){

  ASSERTARGS(tmStruct && timeString);

  char   *beginning = "timestamp=";
  char   *myTimeString;
  char   *allowed = "0123456789:TZ-\134"; /* 134 is Octal for forward slash **/

  /* timeString must start with 'timestamp=' **/
  if(timeString != strstr(timeString, beginning)){
    fprintf(stderr, "parseStateTime(): Error 'timeString' parameter "
	    "does not start with 'timestamp='.\n");
    return ztInvalidArg;
  }

  /* copy DATE/TIME part only into myTimeString;
   * starting after 'timestamp=' **/
  myTimeString = STRDUP(timeString + strlen(beginning));

  /* check for allowed characters **/
  if(strspn(myTimeString, allowed) != strlen(myTimeString)){
    fprintf(stderr, "%s: Error parseStateTime() parameter 'timeString' has "
	    "disallowed character.\n", progName);
    return ztDisallowedChar;
  }

  char  *yearTkn, *monthTkn, *dayTkn,
    *hourTkn, *minuteTkn, *secondTkn;

  int  year, month, day,
    hour, minute, second;

  char  *endPtr;

  char  *delimiter = "-T:Z\134";

  yearTkn = strtok(myTimeString, delimiter);
  if(!yearTkn){
    fprintf(stderr, "%s: Error failed to extract yearTkn.\n", progName);
    return ztParseError;
  }

  monthTkn = strtok(NULL, delimiter);
  if(!monthTkn){
    fprintf(stderr, "%s: Error failed to extract monthTkn.\n", progName);
    return ztParseError;
  }

  dayTkn = strtok(NULL, delimiter);
  if(!dayTkn){
    fprintf(stderr, "%s: Error failed to extract dayTkn.\n", progName);
    return ztParseError;
  }

  hourTkn = strtok(NULL, delimiter);
  if(!hourTkn){
    fprintf(stderr, "%s: Error failed to extract hourTkn.\n", progName);
    return ztParseError;
  }

  minuteTkn = strtok(NULL, delimiter);
  if(!minuteTkn){
    fprintf(stderr, "%s: Error failed to extract minuteTkn.\n", progName);
    return ztParseError;
  }

  secondTkn = strtok(NULL, delimiter);
  if(!secondTkn){
    fprintf(stderr, "%s: Error failed to extract secondTkn.\n", progName);
    return ztParseError;
  }

  year = (int) strtol(yearTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'yearTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, yearTkn);
    return ztParseError;
  }

  month = (int) strtol(monthTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'monthTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, monthTkn);
    return ztParseError;
  }

  day = (int) strtol(dayTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'dayTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, dayTkn);
    return ztParseError;
  }

  hour = (int) strtol(hourTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'hourTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, hourTkn);
    return ztParseError;
  }

  minute = (int) strtol(minuteTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'minuteTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, minuteTkn);
    return ztParseError;
  }

  second = (int) strtol(secondTkn, &endPtr, 10);
  if (*endPtr != '\0'){
    fprintf(stderr, "%s: Error failed strtol() for 'secondTkn' in parseStateTime().\n"
	    " Failed string: <%s>\n", progName, secondTkn);
    return ztParseError;
  }

  /* fill members in struct tm **/
  tmStruct->tm_year = year - 1900;

  tmStruct->tm_mon = month - 1; /* month is zero based **/

  tmStruct->tm_mday = day;

  tmStruct->tm_hour = hour;

  tmStruct->tm_min = minute;

  tmStruct->tm_sec = second;

  return ztSuccess;

} /* END parseStateTime() **/

/* parseSequenceLine(): extracts sequence number from sequence line.
 *
 * function allocates memory and sets sequenceSting pointer.
 *
 *
 ***************************************************************************/

int parseSequenceLine(char **sequenceString, const char *line){

  ASSERTARGS(sequenceString && line);

  /* get sequence number **/
  char   *seqString;
  char   *digits = "0123456789";
  char   *mySeqStr;

  *sequenceString = NULL; /* in case we fail **/

  seqString = strchr(line, '=') + 1;
  if(! seqString){
    fprintf(stderr, "%s: Error failed to extract 'seqString'.\n", progName);
    return ztParseError;
  }

  mySeqStr = STRDUP(seqString);

  removeSpaces(&mySeqStr); /* just in case **/

  /* all characters must be digits **/
  if(strspn(mySeqStr, digits) != strlen(mySeqStr)){
    fprintf(stderr, "%s: Error sequence string has non-digit character.\n", progName);
    return ztParseError;
  }

  /* it can not be more than 9 characters long **/
  if(strlen(mySeqStr) > 9){
    fprintf(stderr, "%s: Error 'sequence string' has more than 9 digits.\n", progName);
    return ztParseError;
  }

  /* set sequenceString destination pointer **/
  *sequenceString = STRDUP(mySeqStr);

  return ztSuccess;

} /* END parseSequenceLine() **/
