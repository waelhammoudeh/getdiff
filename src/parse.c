/*
 * parse.c
 *
 *  Created on: Feb 7, 2022
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

/* setting on the command line override configuration file setting.
* parseCmdLine() function fills found settings on the command, could be
* none, part of or all settings. parseCmdLine() does no error checking.
***********************************************************************/

int parseCmdLine(SETTINGS *ptrSetting, int argc, char* const argv[]){

	static const char *shortOptions = "c:u:p:s:d:b:n:hv";

	static const struct option longOptions[] = {
		{"help", 0, NULL, 'h'},
		{"conf", 1, NULL, 'c'},
		{"user", 1, NULL, 'u'},
		{"passwd", 1, NULL, 'p'},
		{"source", 1, NULL, 's'},
		{"directory", 1, NULL, 'd'},
		{"begin", 1, NULL, 'b'},
		{"verbose", 0, NULL, 'v'},
		{"new", 1, NULL, 'n'},
		{NULL, 0, NULL, 0}
	};

	int 	opt = 0;
	int 	longIndex = 0;
	int 	confFlag, usrFlag, passwdFlag,
	        srcFlag, destFlag, beginFlag, newFlag; /* do not allow same option twice */

	confFlag = usrFlag = passwdFlag = srcFlag = destFlag = beginFlag = newFlag = 0;
	/* This is ugly maybe!
	 * It is easy to specify same option more than once with short option?! */

	do{
		//fetch one option
		opt = getopt_long(argc, argv, shortOptions, longOptions, &longIndex);

		switch(opt){

			case 'h': // TODO: show long help AND exit. No return!

				showHelp();

				break;

			case '?': // TODO: invalid option, show short help AND exit.

				fprintf(stderr, "%s: Error got invalid option in getopt_long(): < %c >.\n", progName, optopt);
				return ztInvalidArg;
				break;

			case 'v':

				ptrSetting->verbose = "yes";
				break;

			case 'c':

				if (confFlag){
					fprintf(stderr, "%s: Error; duplicate conf option!\n", progName);
					return ztInvalidArg;
				}

				if (IsArgUsableFile(optarg) == ztSuccess)

					ptrSetting->conf = strdup(optarg);
				else {

					fprintf(stderr, "%s: parseCmdLine() Error argument to configure option is NOT usable file.\n"
							" Argument: [%s].\n", progName, optarg);
					return ztInvalidArg;
				}

				confFlag = 1;
				break;

			case 'u':

				if (usrFlag){
					fprintf(stderr, "%s: Error; duplicate usr option!\n", progName);
					return ztInvalidArg;
				}

				if (strlen(optarg) > MAX_NAME_LENGTH){

					fprintf(stderr, "%s: parseCmdLine() Error argument to user name option is longer than 64 characters.\n"
							" Argument: [%s].\n", progName, optarg);
					return ztInvalidArg;
				}
				ptrSetting->usr = strdup(optarg);

				usrFlag = 1;
				break;


			case 'p':

				if (passwdFlag){
					fprintf(stderr, "%s: Error; duplicate password option!\n", progName);
					return ztInvalidArg;
				}

				if (strlen(optarg) > MAX_NAME_LENGTH){

					fprintf(stderr, "%s: parseCmdLine() Error argument to password option is longer than 64 characters.\n"
							" Argument: [%s].\n", progName, optarg);
					return ztInvalidArg;
				}
				ptrSetting->psswd = strdup(optarg);

				passwdFlag = 1;
				break;

			case 's':

				if (srcFlag){
					fprintf(stderr, "%s: Error; duplicate source option!\n", progName);
					return ztInvalidArg;
				}

				if (isGoodURL(optarg))

					ptrSetting->src = strdup(optarg);
				else {
					fprintf(stderr, "%s: parseCmdLine() Error argument to source option is NOT valid URL string.\n"
							" Argument: [%s].\n", progName, optarg);
					return ztInvalidArg;
				}

				srcFlag = 1;
				break;

			case 'd':

				if (destFlag){
					fprintf(stderr, "%s: Error; duplicate destination option!\n", progName);
					return ztInvalidArg;
				}

				if ( IsGoodDirectoryName(optarg) )

					ptrSetting->workDir = strdup (optarg);

				else {

            		fprintf (stderr, "%s: parseCmdLine() Error bad directory name specified optarg "
            				": [%s].\n", progName, optarg);
                    return ztInvalidArg;
            	}

				destFlag = 1;
				break;

			case 'b':

				if (beginFlag){
					fprintf(stderr, "%s: Error; duplicate \"begin\" option!\n", progName);
					return ztInvalidArg;
				}

				if (strspn(optarg, "1234567890") == strlen(optarg) &&
					strlen(optarg) == 3 ){

					ptrSetting->start = strdup (optarg);
				}
				else {
            		fprintf (stderr, "%s: parseCmdLine() Error, invalid \"begin\" argument.\n"
            				"Please provide only the 3 digits from the file name.\n"
            				"Invalid argument : [%s].\n", progName,optarg);
                    return ztInvalidArg;
				}

				beginFlag = 1;
				break;

			case 'n':

				if (newFlag){
					fprintf(stderr, "%s: Error; duplicate \"new\" option!\n", progName);
					return ztInvalidArg;
				}

				if ( ! IsGoodFileName (optarg) ){

					fprintf (stderr, "%s: parseCmdLine() Error bad file name specified for new option "
            				": [%s].\n", progName, optarg);
                    return ztInvalidArg;
				}

				ptrSetting->newerFile = strdup (optarg);

				newFlag = 1;
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

	} while (opt != -1); //end do statement

	/* there should be no more arguments left */
	if (optind != argc){
		fprintf(stderr, "%s: Error, more arguments are left after getopt_long().\n", progName);
		return ztInvalidArg;
	}

	return ztSuccess;
}
