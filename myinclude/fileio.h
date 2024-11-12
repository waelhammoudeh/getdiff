/*
 * fileio.h
 *
 *  Created on: Dec 19, 2018
 *      Author: wael
 */

#ifndef FILEIO_H_
#define FILEIO_H_

#include <limits.h>
#include "list.h"


int file2StringList(STRING_LIST *strList, const char *filename);

int stringList2File(const char *filename, STRING_LIST *list);

void printStringList(STRING_LIST *list);

void fprintStringList(FILE *tofile, STRING_LIST *list);

ELEM* findElemSubString (STRING_LIST *list, char *subString);

ELEM* findElemString (STRING_LIST *list, char *string);

int removeFile(const char *filename);

int renameFile(const char *oldName, const char *newName);

int readStartID(char **idString, char *filename);

int writeStartID (char *idStr, char *filename);

int writeNewerFiles(char const *toFile, STRING_LIST *list);

FILE *initialLog(const char *name);

char *readPreviousID(const char *filename);

int writePreviousID (char *idStr, char *filename);

int isOrphanedLock(char *name);

int directoryExist(char *name);

int getLock(int *pUserFD, char *name);

int releaseLock(int fd);


#endif /* FILEIO_H_ */
