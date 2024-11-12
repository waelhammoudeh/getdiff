/*
 * debug.c
 *
 *  Created on: Sep 4, 2024
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "getdiff.h"
#include "util.h"
#include "ztError.h"


void fprintPathPart(FILE *file, PATH_PART *pp){

  ASSERTARGS(pp);

  FILE  *fPointer = stdout;

  if(file)  fPointer = file;

  fprintf(fPointer, "fprintPathPart(): PATH_PART members are:\n");

  fprintf(fPointer, " sequenceNum is: <%s>\n", pp->sequenceNum);

  fprintf(fPointer, " rootEntry is: <%s>\n", pp->rootEntry);

  fprintf(fPointer, " parentEntry is: <%s>\n", pp->parentEntry);

  fprintf(fPointer, " fileEntry is: <%s>\n", pp->fileEntry);

  fprintf(fPointer, " parentPath is: <%s>\n", pp->parentPath);

  fprintf(fPointer, " filePath is: <%s>\n", pp->filePath);

  fprintf(fPointer, "fprintPathPart(): Done.\n\n");

  return;

} /* END fprintPathPart() **/

void fprintStateInfo2(FILE *toStream, STATE_INFO *si){

  FILE *toFP = stdout;

  if(toStream)  toFP = toStream;

  fprintf(toFP, "fprintStateInfo(): STATE_INFO members are:\n");

  fprintf(toFP, "  timeString is: %s\n", si->timeString);

  fprintf(toFP, "  sequenceNum is: %s\n", si->seqNumStr);

  fprintf(toFP, "  isGeofabrik is: %s\n", si->isGeofabrik ? "TRUE" : "FALSE");

  if(si->isGeofabrik)
	fprintf(toFP, "  originalSeqStr is: %s\n", si->originalSeqStr);

  fprintPathPart(toFP, si->pathPart);

  fprintf(toFP, "  year in timestampTM is: %d\n", si->timestampTM->tm_year + 1900);

  fprintf(toFP, "  month in timestampTM is: %d\n", si->timestampTM->tm_mon + 1);

  fprintf(toFP, "  day of month in timestampTM is: %d\n", si->timestampTM->tm_mday);

  fprintf(toFP, "  hour in timestampTM is: %d\n", si->timestampTM->tm_hour);

  fprintf(toFP, "  minutes in timestampTM is: %d\n", si->timestampTM->tm_min);

  fprintf(toFP, "  seconds in timestampTM is: %d\n", si->timestampTM->tm_sec);

  fprintf(toFP, "  timeValue is: %ld\n", si->timeValue);

  fprintf(toFP, "fprintStateInfo(): Done.\n\n");

  return;

} /* END fprintStateInfo2() **/

/* logMessage(): writes string pointed to by 'msg' parameter to the open file
 * with 'to' file pointer.
 * If 'msg' points to the string "START", start header is written, if it points
 * to the string "DONE" tail footer is written, otherwise 'msg' is appended to
 * current time then written to 'to' file.
 *
 * It is an error if 'msg' is empty (string length == 0) or longer than PATH_MAX.
 *
 *****************************************************************************/

int logMessage(FILE *to, char *msg){

  char   *startTemplate =
    "+++++++++++++++++++++++++++++++++++ STARTING +++++++++++++++++++++++++++++++++\n"
    "%s [%d]: %s started. (Version: %s)\n";

  char   *doneTemplate =
    "%s [%d]: %s is done.\n"
    "===================================== DONE ===================================\n\n";

  ASSERTARGS (to && msg);

  if(strlen(msg) == 0){
    fprintf(stderr,"logMessage(): Error 'msg' parameter is empty.\n");
    return ztInvalidArg;
  }

  if(strlen(msg) > PATH_MAX){
    fprintf(stderr,"logMessage(): Error 'msg' parameter is longer than PATH_MAX: <%d>.\n", PATH_MAX);
    return ztInvalidArg;
  }

  char   *timestamp = NULL;
  pid_t   myPID;

  char   tmpBuf[PATH_MAX] = {0};

  timestamp = formatMsgHeadTime(); //vsprintf()
  if(! timestamp){
    fprintf(stderr,"logMessage(): Error failed formatMsgHeadTime().\n");
    return ztMemoryAllocate;
  }

  myPID = getpid();

  if(strcmp(msg, "START") == 0){

    sprintf(tmpBuf, startTemplate, timestamp, (int) myPID, progName, VERSION);
    fprintf(to, tmpBuf);
  }

  else if(strcmp(msg, "DONE") == 0){

    sprintf(tmpBuf, doneTemplate, timestamp, (int) myPID, progName);
    fprintf(to, tmpBuf);
  }

  else{

    fprintf (to, "%s [%d]: %s\n", timestamp, (int) myPID, msg);

  }

  fflush(to);

  return ztSuccess;

} /* END logMessage() **/

#define OK_TEMPLATE "member \"%s\" is set to: %s\n"
#define NEG_TEMPLATE "member \"%s\" is not set.\n"

void fprintSkeleton(FILE *toFP, SKELETON *skl){

  if(!skl){
    fprintf(stderr, "%s: Error fprintSkeleton() parameter 'skl' is NULL.\n", progName);
    return;
  }

  FILE *destFP = stdout;

  if(toFP)
    destFP = toFP;

  fprintf(destFP, "fprintSkeleton(): printing structure 'SKELETON' members below:\n");

  if(skl->workDir)
    fprintf(destFP, OK_TEMPLATE, "workDir", skl->workDir);
  else
    fprintf(destFP, NEG_TEMPLATE, "workDir");

  if(skl->tmp)
    fprintf(destFP, OK_TEMPLATE, "tmp", skl->tmp);
  else
    fprintf(destFP, NEG_TEMPLATE, "tmp");

  if(skl->geofabrik)
    fprintf(destFP, OK_TEMPLATE, "geofabrik", skl->geofabrik);
  else
    fprintf(destFP, NEG_TEMPLATE, "geofabrik");

  if(skl->planet)
    fprintf(destFP, OK_TEMPLATE, "planet", skl->planet);
  else
    fprintf(destFP, NEG_TEMPLATE, "planet");

  if(skl->planetMin)
    fprintf(destFP, OK_TEMPLATE, "planetMin", skl->planetMin);
  else
    fprintf(destFP, NEG_TEMPLATE, "planetMin");

  if(skl->planetHour)
    fprintf(destFP, OK_TEMPLATE, "planetHour", skl->planetHour);
  else
    fprintf(destFP, NEG_TEMPLATE, "planetHour");

  if(skl->planetDay)
    fprintf(destFP, OK_TEMPLATE, "planetDay", skl->planetDay);
  else
    fprintf(destFP, NEG_TEMPLATE, "planetDay");

  fprintf(destFP, "fprintSkeleton() is Done.\n\n");

  return;

} /* END fprintSkeleton() **/

void fprintGdFiles(FILE *toFP, GD_FILES *gdfiles){

  if(!gdfiles){
    fprintf(stderr, "%s: Error fprintGdfiles() parameter 'gdfiles' is NULL.\n", progName);
    return;
  }

  FILE *destFP = stdout;

  if(toFP)
    destFP = toFP;

  fprintf(destFP, "fprintGdfiles(): printing structure 'GD_FILES' members below:\n");

  if(gdfiles->lockFile)
    fprintf(destFP, OK_TEMPLATE, "lockFile", gdfiles->lockFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "lockFile");

  if(gdfiles->logFile)
    fprintf(destFP, OK_TEMPLATE, "logFile", gdfiles->logFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "logFile");

  if(gdfiles->previousSeqFile)
    fprintf(destFP, OK_TEMPLATE, "previousSeqFile", gdfiles->previousSeqFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "previousSeqFile");

  if(gdfiles->prevStateFile)
    fprintf(destFP, OK_TEMPLATE, "prevStateFile", gdfiles->prevStateFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "prevStateFile");

  if(gdfiles->newDiffersFile)
    fprintf(destFP, OK_TEMPLATE, "newDiffersFile", gdfiles->newDiffersFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "newDiffersFile");

  if(gdfiles->rangeFile)
    fprintf(destFP, OK_TEMPLATE, "rangeFile", gdfiles->rangeFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "rangeFile");

  if(gdfiles->latestStateFile)
    fprintf(destFP, OK_TEMPLATE, "latestStateFile", gdfiles->latestStateFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "latestStateFile");

  fprintf(destFP, "fprintGdfiles() is Done.\n\n");

  return;

} /* END fprintGdfiles() **/

void fprintSetting(FILE *toFP, MY_SETTING *settings){

  if(!settings){
    fprintf(stderr, "%s: Error fprintSetting() parameter 'settings' is NULL.\n", progName);
    return;
  }

  FILE *destFP = stdout;

  if(toFP)
    destFP = toFP;

  fprintf(destFP, "fprintSetting(): printing structure 'MY_SETTING' members below:\n");

  if(settings->source)
    fprintf(destFP, OK_TEMPLATE, "source", settings->source);
  else
    fprintf(destFP, NEG_TEMPLATE, "source");

  if(settings->rootWD)
    fprintf(destFP, OK_TEMPLATE, "rootWD", settings->rootWD);
  else
    fprintf(destFP, NEG_TEMPLATE, "rootWD");

  if(settings->configureFile)
    fprintf(destFP, OK_TEMPLATE, "configureFile", settings->configureFile);
  else
    fprintf(destFP, NEG_TEMPLATE, "configureFile");

  if(settings->usr)
    fprintf(destFP, OK_TEMPLATE, "usr", settings->usr);
  else
    fprintf(destFP, NEG_TEMPLATE, "usr");

  if(settings->pswd)
    fprintf(destFP, OK_TEMPLATE, "pswd", "xxxxxxxxx");
  else
    fprintf(destFP, NEG_TEMPLATE, "pswd");

  if(settings->startNumber)
    fprintf(destFP, OK_TEMPLATE, "startNumber", settings->startNumber);
  else
    fprintf(destFP, NEG_TEMPLATE, "startNumber");

  if(settings->endNumber)
    fprintf(destFP, OK_TEMPLATE, "endNumber", settings->endNumber);
  else
    fprintf(destFP, NEG_TEMPLATE, "endNumber");

  if(settings->verbose)
    fprintf(destFP, "member \"verbose\" is On.\n");
  else
    fprintf(destFP, "member \"verbose\" is Off.\n");

  if(settings->newDifferOff)
    fprintf(destFP, "member \"newDifferOff\" is On.\n");
  else
    fprintf(destFP, "member \"newDifferOff\" is Off.\n");

  if(settings->textOnly)
    fprintf(destFP, "member \"textOnly\" is On.\n");
  else
    fprintf(destFP, "member \"textOnly\" is Off.\n");

  fprintf(destFP, "fprintSetting() is Done.\n\n");

  return;

}
