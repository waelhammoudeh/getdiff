/*
 * fileio.h
 *
 *  Created on: Dec 19, 2018
 *      Author: wael
 */

#ifndef FILEIO_H_
#define FILEIO_H_

#include <limits.h>
#include "dList.h"

typedef struct LINE_INFO_ {
	char		*string;
	int		originalNum;
} LINE_INFO;

#ifndef LONG_LINE
	#define LONG_LINE PATH_MAX
#endif

int file2List (DL_LIST *list, char *filename);
void printLineInfo(LINE_INFO *lineInfo);
void printFileList(DL_LIST *list);
void zapLineInfo(void **data);

int liList2File(char *dstFile, DL_LIST *list);

int strList2File(char *dstFile, DL_LIST *list);

void printStringList(DL_LIST *list);

#endif /* SOURCE_FILEIO_H_ */
