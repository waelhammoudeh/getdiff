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

typedef struct LINE_INFO_ {
	char		*string;
	int		originalNum;
} LINE_INFO;

#ifndef LONG_LINE
	#define LONG_LINE PATH_MAX
#endif

int file2List (DLIST *list, char *filename);
void printLineInfo(LINE_INFO *lineInfo);
void printFileList(DLIST *list);
void zapLineInfo(void **data);

int liList2File(char *dstFile, DLIST *list);

int strList2File(char *dstFile, DLIST *list);

void writeDL (FILE *toFile, DLIST *list,
		                void writeFunc (FILE *to, void *data));

#endif /* SOURCE_FILEIO_H_ */
