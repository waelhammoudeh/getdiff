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

int removeFile(const char *filename);

int renameFile(const char *oldName, const char *newName);


#endif /* FILEIO_H_ */
