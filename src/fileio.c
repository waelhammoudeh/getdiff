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

#include "fileio.h"
#include "util.h"  /* includes list.h **/
#include "ztError.h"

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
       (buffer[PATH_MAX] != '\n')){ /* did not read linefeed --> truncated **/

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

  return;

} /* END printStringList() **/

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

  /* we only keep cookie file. remove script and json settings files
   * the right way is to use temporary files.
   ***********************************************************/

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
