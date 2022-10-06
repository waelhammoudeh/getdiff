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
#include <list.h>

#include "fileio.h"
#include "util.h"
#include "ztError.h"

/* file2List(): reads text file named by filename into list,
 * each line is placed into a LINE_INFO structure then data member
 * in list element is set to LINE_INFO pointer;
 * the line itself is cleaned of leading space and line feed then
 * placed into string member of LINE_INFO, original line number from
 * source file is set in originalNum member of LINE_INFO.
 * Comment lines are ignored - comment line starts with [# or ;].
 * list is a pointer to initialized empty list. filename is
 * assumed to exist. Function calls exit() if fopen() fails! THIS HEARTS
 * To avoid exit() call IsArgUsableFile() before calling me.
 * LONG_LINE is defined as PATH_MAX; I think that is very looong line!
 * function allocates memory for each line, use zapLineInfo() as second
 * argument when initializing list, this way zapLineInfo() will be called
 * when you destroy the list.
 * It is an error for list not to be empty.
 *
 *********************************************************************/

int file2List (DLIST *list, char *filename){

	FILE 		*fPtr;
	LINE_INFO 	*newLine;

	char		line[LONG_LINE + 1];
	int			myLineNum = 0;
	char		*start;
	char		SPACE = '\040';
	char		TAB = '\t';
	int			result;

	ASSERTARGS(list && filename); //abort() on NULL pointer argument.


	if(DL_SIZE(list) != 0){
		printf("file2List(): Error argument list not empty.\n");
		return ztListNotEmpty;
	}

	errno = 0;

	//try to open the file for reading - exit() on failure!
	fPtr = fopen(filename, "r");
	if (fPtr == NULL){
		printf ("file2List(): Error opening file: %s. calling exit()\n", filename);
		// print system error
		printf("System error message: %s\n\n", strerror(errno));

		//print reason with perror()
		perror("file2List() call to fopen() failed!");
		exit(ztOpenFileError);
	}

	while (myFgets(line, LONG_LINE, fPtr, &myLineNum)){
		/* FIXME :: trouble if line > LONG_LINE - 1
		 * fgets()  reads in at most one less than size characters from
		 *  stream and stores them into the buffer pointed to by s.
		 *  Reading stops after an EOF or a newline.  If a newline is read,
		 *  it is stored into the buffer.  A  terminating null byte ('\0')
		 *  is stored after the last character in the buffer. <-from man fgets.
		 *  maybe check for \n? or line length with strlen()?
		 *******************************************************************/
		start = line;

		// remove line feed - included by fgets()
		if (start[strlen(start) - 1] == '\n')

			start[strlen(start) - 1] = '\0';

		// move to first non-space character
		while (start[0] == SPACE || start[0] == TAB)

			start++;

		newLine = (LINE_INFO *) malloc(sizeof(LINE_INFO));
		if (newLine == NULL){
			printf("file2List(): Error allocating memory.\n");
			fclose(fPtr);
			return ztMemoryAllocate;
		}

		newLine->string = strdup(start);
		newLine->originalNum = myLineNum;

		result = insertNextDL (list, DL_TAIL(list), newLine);
		if(result != ztSuccess){
			printf("file2List(): Error from ListInsertNext().\n");
			fclose(fPtr);
			return result;
		}

	} //end while()

	fclose(fPtr);

	return ztSuccess;
}

void printLineInfo(LINE_INFO *lineInfo){

	ASSERTARGS(lineInfo);

	printf("%d %s\n", lineInfo->originalNum, lineInfo->string);
	return;
}

void printFileList(DLIST *list){

	ELEM	*start;
	LINE_INFO *lineInfo;


	ASSERTARGS(list);

	if(DL_SIZE(list) == 0){
		printf ("printFileList(): Empty list, nothing to do.\n");
		return;
	}
	else
		printf("printFileList(): List Size is: %d\n", DL_SIZE(list));

	start = DL_HEAD(list);
	while(start){

		lineInfo = start->data;

		printLineInfo(lineInfo);

		start = start->next;
	}
}

/* exact match to destroyDL(), then cast: lineInfo = (LINE_INFO *) *data; */

void zapLineInfo(void **data){

	LINE_INFO *lineInfo;

	ASSERTARGS(data);

	lineInfo = (LINE_INFO *) *data;

	if(lineInfo && lineInfo->string){

		free(lineInfo->string);
		memset(lineInfo, 0, sizeof(LINE_INFO));
	}

	free(lineInfo);

	return;
}

/* liList2File(): line_info list to file. FIXME: neeeeeeds new name
 * function creates file dstFile for writing, writes DLIST of LINE_INFO to it.
 * DLIST contains LINE_INFO structure; ONLY string member is written to file.
 * dstFile is created regardless of list size.
 * checked run time error for dstFile to be good file name.
 * Function returns ztBadFileName, ztCreateFileErr or ztSuccess.
 ******************************************************************************/
int liList2File(char *dstFile, DLIST *list){

	FILE		*filePtr;
	ELEM		*elem;
	LINE_INFO	*lineInfo;

	ASSERTARGS (dstFile && list);

	if (! IsGoodFileName(dstFile))

		return ztBadFileName;

	errno = 0;
	filePtr = fopen ( dstFile, "w");
	if ( filePtr == NULL){
		printf ("list2File(): Error could not create destination file! <%s>\n",
				dstFile);
		printf("System error message: %s\n\n", strerror(errno));
		return ztCreateFileErr;
	}

	if (DL_SIZE(list) == 0){
		fclose (filePtr);
		return ztSuccess;
	}

	elem = DL_HEAD(list);

	while (elem) {

		lineInfo = DL_DATA(elem);
		ASSERTARGS(lineInfo && lineInfo->string);

		fprintf (filePtr, "%s\n", lineInfo->string);

		elem = DL_NEXT(elem);
	}

	fclose (filePtr);

	return ztSuccess;
}

/* just like liList2File() but element data is a pointer to string
 * not LINE_INFO
 */
int strList2File(char *dstFile, DLIST *list){

	FILE		*filePtr;
	ELEM		*elem;
	//LINE_INFO	*lineInfo;
	char		*string;

	ASSERTARGS (dstFile && list);

	if (! IsGoodFileName(dstFile))

		return ztBadFileName;

	errno = 0;
	filePtr = fopen ( dstFile, "w");
	if ( filePtr == NULL){
		printf ("list2File(): Error could not create destination file! <%s>\n",
				dstFile);
		printf("System error message: %s\n\n", strerror(errno));
		return ztCreateFileErr;
	}

	if (DL_SIZE(list) == 0){
		fclose (filePtr);
		return ztSuccess;
	}

	elem = DL_HEAD(list);

	while (elem) {

		string = DL_DATA(elem);
		ASSERTARGS(string);

		fprintf (filePtr, "%s\n", string);

		elem = DL_NEXT(elem);
	}

	fclose (filePtr);

	return ztSuccess;
}

/* toFile: file pointer to an open file, can be NULL -> standard output is used
 * writeFunc() writes specific data type.  */
void writeDL (FILE *toFile, DLIST *list, void writeFunc (FILE *to, void *data)){


	ELEM *elem;
	void			*data;

	ASSERTARGS (list && writeFunc);

	if (DL_SIZE(list) < 1)

		return;

	elem = DL_HEAD(list);
	while (elem) {

		data = (void *) DL_DATA(elem);
		writeFunc (toFile, data);

		elem = DL_NEXT(elem);

	}

	return;

} // END writeDL()
