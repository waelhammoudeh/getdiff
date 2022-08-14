/*
 * util.h
 *
 *  Created on: Jun 23, 2017
 *      Author: wael
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include "dList.h"

#ifndef FALSE
	#define FALSE		0
	#define TRUE		(! FALSE)
#endif

/// Determine whether the given signed or unsigned integer is odd.
#define IS_ODD( num )   ((num) & 1)

/// Determine whether the given signed or unsigned integer is even.
#define IS_EVEN( num )  (!IS_ODD( (num) ))

/// Return min of two numbers. Commonly used but never defined as part of standard headers
#ifndef MIN
#define MIN( n1, n2 )   ((n1) > (n2) ? (n2) : (n1))
#endif

/// Return max of two numbers. Commonly used but never defined as part of standard headers
#ifndef MAX
#define MAX( n1, n2 )   ((n1) > (n2) ? (n1) : (n2))
#endif

#ifndef LONG_LINE

#define LONG_LINE	PATH_MAX

#endif

/* permission for making directory */
#define DIR_PERMISSION   ( S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH )

#define IsSlashEnding(str)    (( (str [strlen(str) - 1]) == '/' ) ? 1 : 0)

/* permission for making directory */
#define PERMISSION   ( S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH )

#define COMMENT_CHAR ';'
#define COMMENT_SET "#;"

/* COMMENT_CHAR and COMMENT_SET are used in myFgets() function, lines starting
 * with such characters are ignored. w.h 12/18/2018 added COMMENT_SET.
 */

/* This is from: WRITING SOLID CODE by Steve Maguire
 * ASSERTARGS: is my function arguments assertion macro. If ARGS_ASSERT is
 * defined it expands to call AssertArgs() function with function name, file
 * name and line number as arguments. AssertArgs outputs an error to "stderr"
 * and aborts. Mainly used to assert arguments to functions, can be used to
 * assert any argument. Can not be used in an expression! This is a statement.
 * The reason for ARGS_ASSERT is that I feel that checking function arguments
 * is a must whether it is DEBUG version or RELEASE version; I abort program
 * in this case because it means there is something wrong with the code, which
 * means all data up to that point is wrong.
*/
#define ARGS_ASSERT

#ifdef ARGS_ASSERT

void AssertArgs (const char *func, char *file, int line);

#define ASSERTARGS(f)	\
	if (f)			\
		{}			\
	else			\
		AssertArgs (__func__, __FILE__,  (int)__LINE__)

#else

#define ASSERTARGS(f)

#endif   /* #ifdef ARGS_ASSERT */


int IsEntryDir (char const *entry);

int IsGoodFileName (char *name);

int IsGoodDirectoryName(char *name);

char* lastOfPath (char *path);

char* DropExtension(char *str);

void** allocate2Dim (int row, int col, size_t elemSize);

void free2Dim (void **array, size_t elemSize);

int removeSpaces(char **str);

char* get_self_executable_directory ();

int IsArgUsableFile(char* const name);

int IsArgUsableDirectory(char* const name);

int myMkDir (char *name);

char* myFgets(char* str, int count, FILE* fPtr, int* lineNum);

int isStrGoodDouble(char *str);

char* getHome();

char *getUserName();

int mySpawn (char *prgName, char **argLst);

int spawnWait (char *prog, char **argsList);

int myGetDirDL (DL_LIST *dstDL, char *dir);

void zapString(void **data);

char* getFormatTime (void);

int stringToUpper (char **dst, char *str);

int stringToLower (char **dest, char *str);

int isGoodURL (const char *str);

char	  *getParentDir (char	*path);

int getDirList (DL_LIST *list, const char *dir);

int isStringInList (DL_LIST *list, const char *str);

int deffList1Not2 (DL_LIST *dst, DL_LIST *list1, DL_LIST *list2);

int convDouble2Long (long *dstL, double srcDouble);

char	 *formatC_Time(void);

char* formatMsgHeadTime(void);

int printBold (char *str);

int mkOutputFile (char **dest, char *givenName, char *rootDir);

FILE* openOutputFile (char *filename);

#endif /* UTIL_H_ */
