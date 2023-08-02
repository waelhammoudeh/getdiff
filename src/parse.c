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

int parseCmdLine(SETTINGS *arguments, int argc, char* const argv[]){

  int    result;
  static const char *shortOptions = "c:u:p:s:d:b:vtn:hV";

  static const struct option longOptions[] = {
    {"version", 0, NULL, 'V'},
    {"conf", 1, NULL, 'c'},
    {"user", 1, NULL, 'u'},
    {"passwd", 1, NULL, 'p'},
    {"source", 1, NULL, 's'},
    {"directory", 1, NULL, 'd'},
    {"begin", 1, NULL, 'b'},
    {"verbose", 0, NULL, 'v'},
    {"new", 1, NULL, 'n'},
    {"text", 0, NULL, 't'},
    {"help", 0, NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int 	opt = 0;
  int 	longIndex = 0;
  int 	confFlag, usrFlag, passwdFlag,
    srcFlag, destFlag, beginFlag,
    newFlag; /* do not allow same option twice */

  confFlag = usrFlag = passwdFlag = srcFlag = destFlag = beginFlag = newFlag = 0;
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

      char  *withPath;

      if(strstr(optarg, "../") && strstr(optarg, "../") == optarg)

    	withPath = prependParent(optarg);

      else if(hasPath(optarg) == FALSE)

        withPath = prependCWD(optarg);

      else

        withPath = optarg;

      if(!withPath){
        fprintf(stderr, "%s: Error 'withPath' variable is NULL in parseCmdLine(); Exiting.\n", progName);
        fprintf(stderr, "optarg was: <%s> @@\n\n", optarg);
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

      arguments->confFile = STRDUP(withPath);

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

      if (isOkayFormat4HTTPS(optarg) == FALSE) {
	fprintf(stderr, "%s: Error invalid source URL string for source option.\n"
		" Argument: [%s].\n", progName, optarg);
	return ztInvalidArg;
      }
      arguments->source = STRDUP(optarg);

      srcFlag = 1;
      break;

    case 'd':
      /* this is the parent (root) for work directory without "gediff" entry **/

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

    case 'n':

      if (newFlag){
	fprintf(stderr, "%s: Error; duplicate \"new\" option!\n", progName);
	return ztInvalidArg;
      }

      /* the words 'none' or 'off' - in any case - turns this off **/
      if( (strcasecmp(optarg, "none") == 0) ||(strcasecmp(optarg, "off") == 0) )

	arguments->noNewDiffers = 1;

      else

	return ztInvalidArg;

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
    fprintf(stderr, "%s: Error, malformed command line.\n"
	    "more arguments are left after getopt_long().\n", progName);
    /* malformed command line, some reason:
     * single dash with long option,
     * space between dashes with long option,
     * lone single dash.
     *********************************************************************/

    return ztMalformedCML;
  }

  return ztSuccess;

}
