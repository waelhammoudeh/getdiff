/*
 * getdiff.h
 *
 *  Created on: Feb 7, 2022
 *      Author: wael
 */

#ifndef GETDIFF_H_
#define GETDIFF_H_

#include "list.h"
#include "configure.h"
#include <curl/curl.h>

#define MAX_NAME_LENGTH 64

extern	 char	*progName;
extern int		flagVerbose;

typedef struct SETTINGS_ {

	char		*usr; // OSM account user name
	char		*psswd;
	char		*src;
	char		*dst;
	char		*workDir;
	char		*conf;
	char		*start;
	char		*startFile;
	char		*tstSrvr;
	char		*scriptFile;
	char		*jsonFile;
	char		*cookieFile;
	char		*logFile;
	char		*htmlFile;
	char		*indexListFile;
	char		*verbose;
	char		*newerFile;

} SETTINGS;

void printSettings(FILE *toFile, SETTINGS *settings);

int updateSettings (SETTINGS *settings, CONF_ENTRY confEntries[]);

int readStart_Id (char **dest, char *filename);

int writeStart_Id (char *idStr, char *filename);

int logMessage (FILE *to, char *txt);

int writeNewerFile (char const *toFile, ELEM *startElem, DLIST *fromList);

int download2FileRetry (FILE *toFilePtr, CURL *handle);

#endif /* GETDIFF_H_ */
