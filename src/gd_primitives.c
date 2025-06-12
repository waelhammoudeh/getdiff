/*
 * gd_functions.c
 *
 *  Created on: May 10, 2025
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ztError.h"
#include "getdiff.h"
#include "curlfn.h"
#include "parse.h"
#include "cookie.h"
#include "fileio.h"
#include "util.h"
#include "debug.h"

int setupFilesys(SKELETON *directories, GD_FILES *files, const char *root){

  int result;

  ASSERTARGS(directories && files && root);

  memset(directories, 0, sizeof(SKELETON));

  result = buildDirectories(directories, root);
  if(result != ztSuccess){
    fprintf (stderr, "%s: Error failed buildFS() function.\n", progName);
    return result;
  }

  memset(files, 0, sizeof(GD_FILES));

  result = setFilenames(files, directories);
  if(result != ztSuccess){
    fprintf (stderr, "%s: Error failed setFilenames() function.\n", progName);
    return result;
  }

  return ztSuccess;

} /* END setupFilesys() **/


/* buildDirectories():
 *  - creates and sets members in dir structure under where:
 *    where/getdiff
 *    where/getdiff/tmp
 *    where/getdiff/geofabrik
 *    where/getdiff/planet
 *    where/getdiff/planet/minute
 *    where/getdiff/planet/hour
 *    where/getdiff/planet/day
 *
 *  Note:
 ********************************************************/

int buildDirectories(SKELETON *dir, const char *where){

  ASSERTARGS(dir && where);

  char *gd = "getdiff/";
  char *gf = "geofabrik/";
  char *plt = "planet/";
  char *plM = "planet/minute/";
  char *plH = "planet/hour/";
  char *plD = "planet/day/";
  char *tmp = "tmp/";

  char buffer[512] = {0};
  char myRoot[512] = {0};

  int numDir = 7;
  int index;

  char *dirArray[numDir];
  int  result;

  if(SLASH_ENDING(where))
    sprintf(myRoot, "%s%s", where, gd);
  else
    sprintf(myRoot, "%s/%s", where, gd);

  dir->workDir = STRDUP(myRoot);

  index = 0;
  dirArray[index] = dir->workDir;

  sprintf(buffer, "%s%s", myRoot, tmp);
  dir->tmp= STRDUP(buffer);

  index++;
  dirArray[index] = dir->tmp;

  sprintf(buffer, "%s%s", myRoot, gf);
  dir->geofabrik = STRDUP(buffer);

  index++;
  dirArray[index] = dir->geofabrik;

  sprintf(buffer, "%s%s", myRoot, plt);
  dir->planet = STRDUP(buffer);

  index++;
  dirArray[index] = dir->planet;

  sprintf(buffer, "%s%s", myRoot, plM);
  dir->planetMin = STRDUP(buffer);

  index++;
  dirArray[index] = dir->planetMin;

  sprintf(buffer, "%s%s", myRoot, plH);
  dir->planetHour = STRDUP(buffer);

  index++;
  dirArray[index] = dir->planetHour;

  sprintf(buffer, "%s%s", myRoot, plD);
  dir->planetDay = STRDUP(buffer);

  index++;
  dirArray[index] = dir->planetDay;

  for(index = 0; index < numDir; index++){
    result = myMkDir(dirArray[index]);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed myMkDir() for directory: <%s>.\n", progName, dirArray[index]);
      return result;
    }
  }

  return ztSuccess;

} /* END buildDirectories() **/

int setFilenames(GD_FILES *gdFiles, SKELETON *dir){

  ASSERTARGS(gdFiles && dir && dir->workDir && dir->tmp);

  gdFiles->lockFile = appendName2Dir(dir->workDir, LOCK_FILE);
  if(! gdFiles->lockFile){
    fprintf(stderr, "%s: Error failed appendName2Dir() for lockFile.\n", progName);
    return ztMemoryAllocate;
  }

  gdFiles->logFile = appendName2Dir(dir->workDir, LOG_NAME);
  if(! gdFiles->logFile){
    fprintf(stderr, "%s: Error failed appendName2Dir() for logFile.\n", progName);
    return ztMemoryAllocate;
  }

  gdFiles->previousSeqFile = appendName2Dir(dir->workDir, PREV_SEQ_FILE);
  if(! gdFiles->previousSeqFile){
    fprintf(stderr, "%s: Error failed appendName2Dir() for previousSeqFile.\n", progName);
    return ztMemoryAllocate;
  }

  gdFiles->prevStateFile = appendName2Dir(dir->workDir, PREV_STATE_FILE);
  if(! gdFiles->prevStateFile){
    fprintf(stderr, "%s: Error failed appendName2Dir() for prevStateFile.\n", progName);
    return ztMemoryAllocate;
  }

  gdFiles->newDiffersFile = appendName2Dir(dir->workDir, NEW_DIFFERS);
  if(! gdFiles->newDiffersFile){
    fprintf(stderr, "%s: Error failed appendName2Dir() for newDiffersFile.\n", progName);
    return ztMemoryAllocate;
  }

  gdFiles->rangeFile = appendName2Dir(dir->workDir, RANGE_FILE);
  if(! gdFiles->rangeFile){
    fprintf(stderr, "%s: Error failed appendName2Dir() for rangeFile.\n", progName);
    return ztMemoryAllocate;
  }

  gdFiles->latestStateFile = appendName2Dir(dir->tmp, LATEST_STATE_FILE);
  if(! gdFiles->latestStateFile){
    fprintf(stderr, "%s: Error failed appendName2Dir() for latestStateFile.\n", progName);
    return ztMemoryAllocate;
  }

  return ztSuccess;

} /* END setFilenames2() **/


PATH_PART *initialPathPart(void){

  PATH_PART *newPP;

  newPP = (PATH_PART*) malloc(sizeof(PATH_PART));

  if( !newPP)

    return newPP;

  memset(newPP, 0, sizeof(PATH_PART));

  return newPP;

} /* END initialPathPart() **/

void zapPathPart(void **pathPart){

  ASSERTARGS(pathPart);

  PATH_PART *myPP;

  myPP = (PATH_PART *) *pathPart;

  if (!myPP) return;

  memset(myPP, 0, sizeof(PATH_PART));

  free(myPP);

  return;

} /* END zapPathPart() **/

STATE_INFO *initialStateInfo(){

  STATE_INFO *tmpSI, *newSI = NULL;

  tmpSI = (STATE_INFO *)malloc(sizeof(STATE_INFO));
  if(!tmpSI)
    return newSI;

  memset(tmpSI, 0, sizeof(STATE_INFO));

  tmpSI->pathPart = (PATH_PART *) initialPathPart();
  if (!tmpSI->pathPart){
    free(tmpSI);
    return newSI;
  }

  tmpSI->timestampTM = (struct tm *)malloc(sizeof(struct tm));
  if(!tmpSI->timestampTM){
    free(tmpSI->pathPart);
    free(tmpSI);
    return newSI;
  }

  newSI = tmpSI;

  return newSI;

} /* END initialStateInfo2() **/

void zapStateInfo(STATE_INFO **si){

  STATE_INFO *mySI = *si;

  if(!mySI) return;

  if(mySI->timestampTM){
    memset(mySI->timestampTM, 0, sizeof(struct tm));
    free(mySI->timestampTM);
  }

  if(mySI->pathPart){
    memset(mySI->pathPart, 0, sizeof(PATH_PART));
    free(mySI->pathPart);
  }

  memset(mySI, 0, sizeof(STATE_INFO));
  free(mySI);

  return;

} /* END zapStateInfo() **/

/* stateFile2StateInfo(): reads & parses 'state.txt' pointed to by filename
 * then fills STATE_INFO structure pointed to by 'stateInfo'.
 *
 * caller initials 'stateInfo'.
 *
 *************************************************************************/

int stateFile2StateInfo(STATE_INFO *stateInfo, const char *filename){

  int   result;

  ASSERTARGS(stateInfo && filename);

  /* read state.txt file into a STRING_LIST **/
  STRING_LIST   *fileStrList;

  fileStrList = initialStringList();
  if( ! fileStrList){
    fprintf(stderr, "%s: Error failed initialStringList() function.\n", progName);
    logMessage(fLogPtr, "Error failed initialStringList() function.");

    return ztMemoryAllocate;
  }

  result = file2StringList(fileStrList, filename);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed file2StringList() function.\n", progName);
    logMessage(fLogPtr, "Error failed file2StringList() function.");
    zapStringList((void **) &fileStrList);

    return result;
  }

  /* file2StringList() returns an error when 'filename' is empty,
   * but check list anyway **/
  if(isStateFileList(fileStrList) == FALSE){
    fprintf(stderr, "%s: Error failed isStateFileList() test.\n", progName);
    logMessage(fLogPtr, "Error failed isStateFileList() test.");
    zapStringList((void **) &fileStrList);

    return ztInvalidArg;
  }

  /* set character pointers for lines (strings) to parse **/
  char  *timeLine = NULL;
  char  *sequenceLine = NULL;
  char  *originalSeqLine = NULL;

  char *timeMark = "timestamp=";
  char *seqMark = "sequenceNumber=";

  char *originalPrefix = "# original OSM minutely replication sequence number";

  ELEM  *elem = NULL;
  char  *line = NULL;

  elem = DL_HEAD(fileStrList);
  while(elem){

    line = (char *) DL_DATA(elem);

    if(strncmp(line, timeMark, strlen(timeMark)) == 0)
      timeLine = line;

    if(strncmp(line, seqMark, strlen(seqMark)) == 0)
      sequenceLine = line;

    if(strncmp(line, originalPrefix, strlen(originalPrefix)) == 0)

      originalSeqLine = line;

    elem = DL_NEXT(elem);
  }

  if(!(timeLine && sequenceLine)){
    fprintf(stderr, "%s: Error failed to retrieve required time line and/or sequence line.\n", progName);
    logMessage(fLogPtr, "Error failed to retrieve required time line and/or sequence line.");

    zapStringList((void **) &fileStrList);
    return ztMalformedFile;
  }

  /* extract data from lines **/
  if(originalSeqLine){

    char *originalSeqNum = originalSeqLine + strlen(originalPrefix) + 1;

    if( ! isGoodSequenceString(originalSeqNum)){
      fprintf(stderr, "%s: Error invalid sequence string in original sequence number.\n", progName);
      logMessage(fLogPtr,"Error invalid sequence string in original sequence number.");
      zapStringList((void **) &fileStrList);

      return ztParseError;
    }

    strcpy(stateInfo->originalSeqStr, originalSeqNum);
    stateInfo->isGeofabrik = 1;

  }

  char *sequenceString;

  sequenceString = sequenceLine + strlen("sequenceNumber=");

  if( ! isGoodSequenceString(sequenceString)){
    fprintf(stderr, "%s: Error invalid sequence string in sequence line.\n", progName);
    logMessage(fLogPtr,"Error invalid sequence string in sequence line.");
    zapStringList((void **) &fileStrList);

    return ztParseError;
  }

  strcpy(stateInfo->seqNumStr, sequenceString);

  result = parseTimestampLine(stateInfo->timestampTM, timeLine);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed parseStateTime() function.\n", progName);
    logMessage(fLogPtr, "Error failed parseStateTime() function.");
    zapStringList((void **) &fileStrList);

    return result;
  }

  /* parsed timeLine okay; set string **/
  char *timeString;

  timeString = timeLine + strlen("timestamp=");

  strcpy(stateInfo->timeString, timeString);

  /* convert tm structure to time value;
   * storing result in timeValue member
   * check returned value from makeTimeGMT()
   ******************************************/

  stateInfo->timeValue = makeTimeGMT(stateInfo->timestampTM);
  if(stateInfo->timeValue == -1){
    fprintf(stderr, "%s: Error failed makeTimeGMT() function.\n", progName);
    logMessage(fLogPtr, "Error failed makeTimeGMT() function.");
    zapStringList((void **) &fileStrList);

    return ztInvalidArg; // function fails with invalid value (in any member)
  }

  result = sequence2PathPart(stateInfo->pathPart, stateInfo->seqNumStr);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed sequence2PathPart() function.\n", progName);
    logMessage(fLogPtr, "Error failed sequence2PathPart() function.");
    zapStringList((void **) &fileStrList);

    return result;
  }

  zapStringList((void **) &fileStrList);
  return ztSuccess;

} /* END stateFile2StateInfo() **/

/* stateFile2SequenceString():
 * returns sequence string from state file with 'filename'.
 *
 ************************************************************/

char *stateFile2SequenceString(const char *filename){

  ASSERTARGS(filename);

  char *sequenceString = NULL;
  int  result;

  STATE_INFO *si;

  si = initialStateInfo();
  if(!si){
    fprintf(stderr, "%s: Error failed initialStateInfo().\n", progName);
    logMessage(fLogPtr, "Error failed initialStateInfo().");

    return sequenceString;
  }

  result = stateFile2StateInfo(si, filename);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed stateFile2StateInfo().\n", progName);
    logMessage(fLogPtr, "Error failed stateFile2StateInfo().");

    return sequenceString;
  }

  sequenceString = STRDUP(si->seqNumStr);

  zapStateInfo(&si);

  return sequenceString;

} /* END stateFile2SequenceString() **/

/* sequence2PathPart(): converts sequence number into file system path parts.
 *
 * Parameters:
 *   pathPart: pointer to PATH_PART structure; caller initials pathPart.
 *   sequenceStr: character pointer to valid Sequence Number string.
 *
 * function replaces seq2PathParts().
 *
 ***********************************************************************/

int sequence2PathPart(PATH_PART *pathPart, const char *sequenceStr){

  ASSERTARGS(pathPart && sequenceStr);

  if(! isGoodSequenceString(sequenceStr)){
    fprintf(stderr, "%s: Error in sequence2PathPart() parameter 'sequenceStr': <%s>\n"
            "failed isGoodSequenceString().\n", progName, sequenceStr);
    return ztInvalidArg;
  }

  memset(pathPart, 0, sizeof(PATH_PART)); /* zero-out all members **/

  /* to format with leading character using printf(), variable must be integer type **/
  int    seqNum;
  char   *endPtr;

  seqNum = (int) strtol(sequenceStr, &endPtr, 10);
  if( *endPtr != '\0'){
    fprintf(stderr, "%s: Error in sequence2PathPart() failed strtol() for sequenceStr.\n", progName);
    return ztInvalidArg;
  }

  /* make a string from seqNum with leading zeros **/
  char   tmpBuf[10] = {0};

  sprintf(tmpBuf, "%09d", seqNum);

  strcpy(pathPart->sequenceNum, sequenceStr); /* copy as received, NOT as formatted **/

  /* make Entry members from formatted string **/

  strncpy(pathPart->rootEntry, tmpBuf, 3);

  pathPart->rootEntry[3] = '/';
  pathPart->rootEntry[4] = '\0';

  strncpy(pathPart->parentEntry, tmpBuf + 3, 3);

  pathPart->parentEntry[3] = '/';
  pathPart->parentEntry[4] = '\0';

  strncpy(pathPart->fileEntry, tmpBuf + 6, 3);

  /* make members parentPath & filePath from entries above **/
  snprintf(pathPart->parentPath, sizeof(pathPart->parentPath), "/%s%s", pathPart->rootEntry, pathPart->parentEntry);

  snprintf(pathPart->filePath, sizeof(pathPart->filePath), "%s%s", pathPart->parentPath, pathPart->fileEntry);

  return ztSuccess;

} /* END sequence2PathPart() **/

int makeOsmDir(PATH_PART *startPP, PATH_PART *latestPP, const char *rootDir){

  int    result;
  char   buffer[PATH_MAX] = {0};

  ASSERTARGS(startPP && latestPP && rootDir);

  /* startPP & latestPP must share rootEntry **/
  if(strcmp(startPP->rootEntry, latestPP->rootEntry) != 0){
    fprintf(stderr, "%s: Error rootEntry is not the same for parameters.\n", progName);
    return ztInvalidArg;
  }

  /* rootDir parameter must exist and accessible **/
  result = isDirUsable(rootDir);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isDirUsable() for parameter 'rootDir'.\n", progName);
    return result;
  }

  if(SLASH_ENDING(rootDir))
    sprintf(buffer, "%s%s", rootDir, startPP->rootEntry);
  else
    sprintf(buffer, "%s/%s", rootDir, startPP->rootEntry);

  result = myMkDir(buffer);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myMkDir() function.\n", progName);
    return result;
  }

  if(SLASH_ENDING(rootDir))
    sprintf(buffer, "%s%s", rootDir, startPP->parentPath);
  else
    sprintf(buffer, "%s/%s", rootDir, startPP->parentPath);

  result = myMkDir(buffer);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myMkDir() function.\n", progName);
    return result;
  }

  if(strcmp(startPP->parentEntry, latestPP->parentEntry) == 0)

    return ztSuccess;

  if(SLASH_ENDING(rootDir))
    sprintf(buffer, "%s%s", rootDir, latestPP->parentPath);
  else
    sprintf(buffer, "%s/%s", rootDir, latestPP->parentPath);

  result = myMkDir(buffer);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed myMkDir() function.\n", progName);
    return result;
  }

  return ztSuccess;

} /* END makeOsmDir() **/

void zapSetting(MY_SETTING *settings){

  ASSERTARGS(settings);

  if(settings->source)
	free(settings->source);

  if(settings->rootWD)
	free(settings->rootWD);

  if(settings->configureFile)
	free(settings->configureFile);

  if(settings->usr)
	free(settings->usr);

  if(settings->pswd)
	free(settings->pswd);

  if(settings->startNumber)
	free(settings->startNumber);

  if(settings->endNumber)
	free(settings->endNumber);

  memset(settings, 0, sizeof(MY_SETTING));

  return;
}

void zapSkeleton(SKELETON *skel){

  ASSERTARGS(skel);

  if(skel->workDir)
	free(skel->workDir);

  if(skel->tmp)
	free(skel->tmp);

  if(skel->geofabrik)
	free(skel->geofabrik);

  if(skel->planet)
	free(skel->planet);

  if(skel->planetMin)
	free(skel->planetMin);

  if(skel->planetHour)
	free(skel->planetHour);

  if(skel->planetDay)
	free(skel->planetDay);

  memset(skel, 0, sizeof(SKELETON));

  return;
}

void zapGd_files(GD_FILES *gf){

  if(gf->lockFile)
	free(gf->lockFile);

  if(gf->logFile)
	free(gf->logFile);

  if(gf->previousSeqFile)
	free(gf->previousSeqFile);

  if(gf->prevStateFile)
	free(gf->prevStateFile);

  if(gf->newDiffersFile)
	free(gf->newDiffersFile);

  if(gf->rangeFile)
	free(gf->rangeFile);

  if(gf->latestStateFile)
	free(gf->latestStateFile);

  memset(gf, 0, sizeof(GD_FILES));

  return;
}


