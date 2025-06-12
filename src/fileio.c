/*
 * fileio.c
 *
 *  Created on: Dec 19, 2018
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "fileio.h"
#include "util.h"  /* includes list.h **/
#include "ztError.h"

#include "getdiff.h"

#include "debug.h"

/* file2StringList(): reads file into string list.
 *
 * caller initials 'strList'.
 **************************************************************/

int file2StringList(STRING_LIST *strList, const char *filename){

  int   result;
  FILE  *fPtr;
  char  buffer[PATH_MAX + 1];
  char  *newString;

  ASSERTARGS(strList && filename);

  if(DL_SIZE(strList) != 0){

    fprintf(stderr, "file2StringList(): Error argument 'strList' is not empty.\n");
    return ztListNotEmpty;
  }

  if(strList->listType != STRING_LT) /* just set it, keep old code working **/

    strList->listType = STRING_LT;

  result = isFileReadable(filename);
  if(result != ztSuccess){

    fprintf(stderr, "file2StringList(): Error failed isFileReadable()"
	    " for argument 'filename'.\n");
    return result;
  }

  errno = 0;

  fPtr = fopen(filename, "r");
  if (fPtr == NULL){

    fprintf (stderr, "file2StringList(): Error failed fopen() function.\n");
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  while (fgets(buffer, (PATH_MAX + 1), fPtr)){

    /* do not allow a line longer than (PATH_MAX) - we do not combine lines. **/
    if((strlen(buffer) == PATH_MAX) &&
       (buffer[PATH_MAX - 1] != '\n')){ /* did not read linefeed --> truncated **/

      fprintf(stderr, "file2StringList(): Error long line; longer than <%d> characters.\n"
	      "lines are not combined by this function.\n", PATH_MAX);
      return ztInvalidArg;
    }

    /* remove line feed - kept by fgets() **/
    if (buffer[strlen(buffer) - 1] == '\n')

      buffer[strlen(buffer) - 1] = '\0';

    newString = STRDUP(buffer);

    /* remove leading and trailing white spaces **/
    removeSpaces(&newString);

    result = insertNextDL (strList, DL_TAIL(strList), (void *)newString);
    if(result != ztSuccess){

      fprintf(stderr, "file2StringList(): Error failed insertNextDL() function.\n");
      fclose(fPtr);
      return result;
    }

  } /* end while() **/

  fclose(fPtr);

  return ztSuccess;

} /* END file2StringList() **/


void printStringList(STRING_LIST *list){

  ELEM   *elem;
  char   *string;


  ASSERTARGS(list);

  if(DL_SIZE(list) == 0){
    printf ("printStringList(): Empty list, nothing to do.\n");
    return;
  }
  else
    printf("printStringList(): List Size is: %d\n", DL_SIZE(list));

  elem = DL_HEAD(list);
  while(elem){

    string = (char *)DL_DATA(elem);

    fprintf(stdout, "%s\n", string);

    elem = DL_NEXT(elem);
  }

  return;

} /* END printStringList() **/

void fprintStringList(FILE *tofile, STRING_LIST *list){

  ELEM   *elem;
  char   *string;

  FILE   *stream = stdout;

  ASSERTARGS(list);

  if(tofile)
    stream = tofile;

  if(DL_SIZE(list) == 0){
    fprintf (stream, "fprintStringList(): Empty list, nothing to print.\n");
    return;
  }
  else
    fprintf(stream, "fprintStringList(): Printing List with size is: <%d>\n", DL_SIZE(list));

  elem = DL_HEAD(list);
  while(elem){

    string = (char *)DL_DATA(elem);

    fprintf(stream, "%s\n", string);

    elem = DL_NEXT(elem);
  }

  fprintf(stream, "fprintStringList(): Done.\n");

  fflush(stream);

  return;

} /* END printStringList() **/

/* findElemSubString(): 'subString' can be anywhere WITHIN string.
 *
 * Note: NOT exact match; uses strstr() to find sub-string.
 *******************************************************************/
ELEM* findElemSubString (STRING_LIST *list, char *subString){

  ELEM  *elem = NULL;
  ELEM  *currentElem;
  char  *string;

  ASSERTARGS(list && subString);

  if(! TYPE_STRING_LIST(list))

    return elem;

  if(DL_SIZE(list) == 0)

    return elem;

  currentElem = DL_HEAD(list);
  while(currentElem){

    string = (char *)DL_DATA(currentElem);

    if(strstr(string, subString)){

      elem = currentElem;
      break;
    }

    currentElem = DL_NEXT(currentElem);
  }

  return elem;

} /* END findElemSubString() **/

/* findElemString(): find element with 'string' exact match.
 *
 * Note: findElemSubString() finds element with SUB-STRING.
 *
 *********************************************************/
ELEM* findElemString (STRING_LIST *list, char *string){

  ELEM  *elem = NULL;
  ELEM  *currentElem;
  char  *elemString;

  ASSERTARGS(list && string);

  if(! TYPE_STRING_LIST(list))

    return elem;

  if(DL_SIZE(list) == 0)

    return elem;

  currentElem = DL_HEAD(list);
  while(currentElem){

    elemString = (char *)DL_DATA(currentElem);

    if(strcmp(string, elemString) == 0){

      elem = currentElem;
      break;
    }

    currentElem = DL_NEXT(currentElem);
  }

  return elem;

} /* END findElemString() **/

/* stringList2File(): writes string list to named file.
 *
 * if list is empty an empty file IS created - WRONG?
 *
 ***********************************************************/

int stringList2File(const char *filename, STRING_LIST *list){

  int    result;
  FILE   *fPtr;
  ELEM   *elem;
  char   *string;

  ASSERTARGS (filename && list);

  result = isGoodFilename(filename);
  if(result != ztSuccess){
    fprintf(stderr, "stringList2File() Error failed isGoodFilename() for 'filename': <%s>\n",
	    filename);
    return result;
  }

  errno = 0;
  fPtr = fopen(filename, "w");
  if(!fPtr){
    fprintf(stderr, "stringList2File(): Error failed fopen() function for 'filename': <%s>\n",
	    filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  if (DL_SIZE(list) == 0){
    fclose(fPtr);
    return ztSuccess;
  }

  elem = DL_HEAD(list);

  while (elem) {

    string = (char *)DL_DATA(elem);

    if(!string){
      fprintf(stderr, "stringList2File(): Error variable 'string' is null ...\n");
      return ztFatalError;
    }

    fprintf (fPtr, "%s\n", string);

    elem = DL_NEXT(elem);

  }

  fclose(fPtr);

  return ztSuccess;

} /* END stringList2File() **/

int removeFile(const char *filename){

  int  result;

  ASSERTARGS(filename);

  errno = 0;

  result = remove(filename);
  if (result != 0){
    fprintf(stderr, "removeFile(): Error failed remove() system call! filename: <%s>\n",
            filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztFailedSysCall;
  }

  return ztSuccess;

} /* END removeFile() **/

int renameFile(const char *oldName, const char *newName){

  int   result;

  ASSERTARGS(oldName && newName);

  /* renameFile() renames files only, NOT directory
   *
   ************************************************/

  if (!isRegularFile(oldName)){
    fprintf(stderr, "renameFile(): Error - '%s' is not a regular file.\n", oldName);
    return ztNotRegFile;
  }

  /* try to use rename() first, we will be done if it is successful **/
  result = rename(oldName, newName);
  if(result == ztSuccess)

    return ztSuccess;

  /* rename() failed; try to read file into list then write list with new name **/

  STRING_LIST   *fileList;

  fileList = initialStringList();
  if(!fileList){
    fprintf(stderr, "renameFile(): Error failed initialStringList(); can not move/rename file!\n");
    return ztMemoryAllocate;
  }

  result = file2StringList(fileList, oldName);
  if(result != ztSuccess){
    fprintf(stderr, "renameFile: Error failed file2StringList(); can not move/rename file!\n");
    return result;
  }

  result = stringList2File(newName, fileList);
  if(result != ztSuccess){
    fprintf(stderr, "renameFile: Error failed stringList2File(); can not move/rename file!\n");
    return result;
  }

  removeFile(oldName);

  return ztSuccess;

} /* END renameFile() **/


/* readStartID() :
 *
 */

int readStartID(char **idString, char *filename){

  FILE  *fPtr;
  char  line[256] = {0};
  int   count = 0;
  char  *str;

  ASSERTARGS (idString && filename);

  *idString = NULL;

  errno = 0;
  fPtr = fopen(filename, "r");
  if(!fPtr){
    fprintf(stderr, "%s: Error failed to open file <%s> in readStartID().\n",
            progName, filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  while(fgets(line, 256, fPtr)){

    count++;
    if (count > 1) /* read one line only **/
      break;
  }

  fclose(fPtr);

  if(count > 1)

    return ztMalformedFile;


  if (line[strlen(line) - 1] == '\n') /* remove linefeed */

    line[strlen(line) -1] = '\0';

  str = STRDUP(line);

  removeSpaces(&str);

  if(isGoodSequenceString(str) != TRUE){
    fprintf (stderr, "%s: Error invalid sequence string: <%s> in start file <%s>\n",
	     progName, str, filename);
    return ztMalformedFile;
  }

  *idString = STRDUP(str);

  free(str);

  return ztSuccess;

} /* END readStartID() **/

int writeStartID (char *idStr, char *filename){

  FILE   *fPtr;

  ASSERTARGS (idStr && filename);

  if(isGoodSequenceString(idStr) != TRUE){
    fprintf (stderr, "%s: Error invalid sequence string in parameter 'idStr'.\n", progName);
    return ztInvalidArg;
  }

  errno = 0;
  fPtr = fopen(filename, "w");
  if (!fPtr){
    fprintf (stderr, "%s: Error failed fopen() for file! <%s>\n", progName, filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  fprintf(fPtr, "%s\n", idStr); /* we write linefeed **/

  fclose(fPtr);

  return ztSuccess;

} /* END writeStartID() **/

/* writeNewerFile():
 *
 * initial string list
 *
 * if toFile exist (usable) -> read file into string list
 *
 * append completedList to string list
 *
 * write string list to file.
 *
 *
 *************************************************************************/

int writeNewerFiles(char const *toFile, STRING_LIST *list){

  ASSERTARGS(toFile && list);

  if(DL_SIZE(list) < 2){
    fprintf(stderr, "%s: Error list size is less than 2 in 'list' parameter.\n", progName);
    return ztInvalidArg;
  }

  if((DL_SIZE(list) % 2 != 0)){ /* size should be even **
	                               use #define IS_EVEN( num ) from util.h **/
    fprintf(stderr, "%s: Error list size is not multiple of 2.\n", progName);
    return ztInvalidArg;
  }

  STRING_LIST   *fileList;
  int           result;
  ELEM          *elem;
  char          *line;

  fileList = initialStringList();
  if(!fileList){
    fprintf(stderr, "%s: Error failed initialStringList() function.\n", progName);
    return ztMemoryAllocate;
  }


  result = isFileUsable(toFile);
  if(result == ztSuccess){
    result = file2StringList(fileList, toFile);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed file2StringList() function.\n", progName);
      return result;
    }
  }

  elem = DL_HEAD(list);
  while(elem){

    line = (char *)DL_DATA(elem);

    result = insertNextDL(fileList, DL_TAIL(fileList), (void *) line);
    if(result != ztSuccess){
      fprintf(stderr, "%s: Error failed inesrtNextDL() function.\n", progName);
      return result;
    }

    elem = DL_NEXT(elem);
  }

  result = stringList2File(toFile, fileList);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed stringList2File() function.\n", progName);
    return result;
  }

  return ztSuccess;

} /* END writeNewerFiles()  **/

FILE *initialLog(const char *name){

  ASSERTARGS(name);

  int  result;
  FILE *filePtr = NULL;

  result = isGoodFilename(name);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error failed isGoodFilename() in initalLog(). "
    		"Parameter: <%s>.\n", progName, name);
    return filePtr;
  }

  errno = 0;
  filePtr = fopen (name, "a");
  if (! filePtr ){
    fprintf (stderr, "%s: Error fopen() in initalLog()! Parameter: <%s>\n",
		     progName, name);
    fprintf(stderr, " System error message: %s\n\n", strerror(errno));
	return filePtr;
  }

  /* write log start header **/
  logMessage(filePtr, "START");

  return filePtr;

}

char *readPreviousID(const char *filename){

  FILE  *fPtr;
  int   bufSize = 16;
  char  line[16] = {0};
  int   count = 0;
  char  *ID_Str = NULL;

  ASSERTARGS (filename);

  errno = 0;
  fPtr = fopen(filename, "r");
  if(!fPtr){
    fprintf(stderr, "%s: Error failed to open file <%s> in readStartID().\n",
            progName, filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ID_Str;
  }

  while(fgets(line, bufSize - 1, fPtr)){

    count++;
    if (count > 1) /* read one line only **/
      break;
  }

  fclose(fPtr);

  if(count > 1)

    return ID_Str;


  if (line[strlen(line) - 1] != '\n') /* last character must be line feed **/

	return ID_Str;

  line[strlen(line) -1] = '\0'; /* remove line feed character **/

  // removeSpaces() -- no spaces to remove, we write with no spaces!

  if(isGoodSequenceString(line) != TRUE){
    fprintf (stderr, "%s: Error invalid sequence string: <%s> in file <%s>\n",
	     progName, line, filename);
    return ID_Str;
  }

  ID_Str = STRDUP(line);

  return ID_Str;

} /* END readPreviousID() **/

int writePreviousID (char *idStr, char *filename){

	return writeStartID (idStr, filename);
}

/* isOrphanedLock():
 *
 * Parameter name is for locked file where owner writes its PID to.
 *
 * In Linux each process has a directory under /proc/
 * with that process PID when running.
 * WE ASSUME WE CAN READ /proc/ directory.
 *
 *
 ****************************************************/
int isOrphanedLock(char *name){

  ASSERTARGS(name);

  int fd;
  int length;
  char buffer[128] = {0};

  int  otherPid;
  char owner[32] = {0};
  char ownerDir[64] = {0};

  fd = open(name, O_RDONLY);
  if(fd == -1){ // failed open() for read-only --> file does NOT exist
    fprintf(stderr, "%s: Error failed to open file in isOrphanedLock().\n", progName);
    return FALSE;
  }

  length = read(fd, buffer, sizeof(buffer));
  close(fd);

  if(length == -1){ // failed to read
    fprintf(stderr, "%s: Error, failed read() function.\n", progName);
    return FALSE;
  }
  else if(length == 0){
    fprintf(stderr, "%s: Error, encountered EOF - file is empty!\n", progName);
    return FALSE;
  }
  else
    ;

  /* note that read() does NOT NUL-terminate string, we could use 'length'
   * to do that, but our buffer is zeroed-out going in!
   *********************************************************************/

  /* read progName then PID from stored buffer **/
  sscanf(buffer, "%s %d", owner, &otherPid);

  sprintf(ownerDir, "/proc/%d/", otherPid);

  if(! directoryExist(ownerDir))

    return TRUE;

  return FALSE;

} /* END isOrphanedLock() **/

int directoryExist(char *name){

  ASSERTARGS(name);

  struct stat status;
  int result;

  result = stat(name, &status);
  if((result == 0) && (S_ISDIR (status.st_mode)) == TRUE)

    return TRUE;

  else

    return FALSE;

} /* END directoryExist() **/

int getLock(int *pUserFD, char *name){

  ASSERTARGS(pUserFD && name);

  int    fd;
  int    result;
  char   writeBuffer[64] = {0};

  *pUserFD = -1; /* initially set to invalid value **/

  struct flock myLock;

  errno = 0;

  /* open file with create (if it does not exist) for reading and writing **/
  fd = open(name, O_CREAT | O_RDWR, 0644);
  if(fd == -1){
    fprintf(stderr, "%s: Error failed open() for parameter 'name' file.\n"
            "System error message: <%s>\n", progName, strerror(errno));
    return ztOpenFileError;
  }

  /* initialize myLock structure **/
  memset(&myLock, 0, sizeof(myLock));

  myLock.l_type = F_WRLCK;
  myLock.l_whence = SEEK_SET; // Start from the beginning of the file
  myLock.l_start = 0;         // Lock from the start
  myLock.l_len = 0;           // Lock the entire file

  /* errno is already set to zero **/

  result = fcntl(fd, F_SETLK, &myLock);
  if (result != 0){

    fprintf(stderr, "%s: Failed to acquire lock file....\n"
            "System error message: %s\n", progName, strerror(errno));

    close(fd);
    return ztFailedSysCall;
  }

  /* write information which may help debug if needed;
   * remove old info first **/
  ftruncate(fd, 0);

  sprintf(writeBuffer, "%s %d", progName, (int) getpid());
  write(fd, writeBuffer, strlen(writeBuffer));

  /* set user pointer for return **/
  *pUserFD = fd;

  return ztSuccess;

} /* END getLock2() **/

int releaseLock(int fd){

  int result;

  struct flock myLock;

  memset(&myLock, 0, sizeof(myLock));
  myLock.l_type = F_UNLCK;
//  myLock.l_whence = SEEK_SET;
  myLock.l_start = 0;
  myLock.l_len = 0;

  errno = 0;

  result = fcntl(fd, F_SETLK, &myLock);
  if(result != 0){
    fprintf(stderr, "%s: Failed to unlock file!\n"
            "System call failed for: %s\n", progName, strerror(errno));
    return ztFailedSysCall;
  }

  sleep(1);

  result = close(fd);
  if(result != 0){
    fprintf(stderr, "%s: Failed close(fd) call!\n"
            "System call failed for: %s\n", progName, strerror(errno));
    return ztFailedSysCall;
  }

  return ztSuccess;

} /* END releaseLock() **/

/* we can get info about locks;;; need fd first!
 * this is using F_GETLK to retrieve lock info
    memset(&myLock, 0, sizeof(myLock));

    result = fcntl(fd, F_GETLK, &myLock);
    if(result != 0){
    fprintf(stderr, "%s: Failed to F_GETLK ....\n", progName);
    return ztFailedSysCall;
    }

    if(myLock.l_type == F_WRLCK)
    fprintf(stdout, "lock type is: F_WRLCK.\n");

    else if(myLock.l_type == F_RDLCK)
    fprintf(stdout, "lock type is: F_RDLCK.\n");

    else if(myLock.l_type == F_UNLCK)
    fprintf(stdout, "lock type is: F_UNLCK.\n");

    else
    fprintf(stdout, "lock type is: none ...\n");

    fprintf(stdout, "PID in myLock is: %d\n", (int) myLock.l_pid);
*/

