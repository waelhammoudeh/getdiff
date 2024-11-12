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
#include <limits.h>

#ifndef DLIST_H_
#include "list.h"
#endif

/* commonly used defines **/

#ifndef TRUE

#define FALSE		0
#define TRUE  (! FALSE)

#endif

/* Determine whether the given signed or unsigned integer is odd or even/ **/
#define IS_ODD( num )   ((num) & 1)
#define IS_EVEN( num )  (!IS_ODD( (num) ))

/* Return minimum or maximum of two numbers. **/
#ifndef MIN
#define MIN(n1, n2) ((n1) > (n2) ? (n2) : (n1))
#endif

#ifndef MAX
#define MAX(n1, n2) ((n1) > (n2) ? (n1) : (n2))
#endif

/* maximum string length for one entry in filename & path.
 * NOTE: ONE entry only string length - one part of path or file name alone.
 * NOTE: FILENAME_MAX is defined somewhere in GNU library to be PATH_MAX.
 *
 * FIXME:: to be removed (use MAX_ENTRY_LENGTH instead.
 *
 **********************************************************************/
#define FNAME_MAX 255


#define MAX_ENTRY_LENGTH 255

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* sane mode for making directory */
#define MK_DIR_MODE   (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

/* is slash ending path? **/
#define SLASH_ENDING(path) (( (path [strlen(path) - 1]) == '/' ) ? 1 : 0)

/* this is to be removed **/
#define IsSlashEnding(str)    (( (str [strlen(str) - 1]) == '/' ) ? 1 : 0)

/* This is from: WRITING SOLID CODE by Steve Maguire
 * ASSERTARGS: is my function arguments assertion macro. If ARGS_ASSERT is
 * defined it expands to call AssertArgs() function with function name, file
 * name and line number as arguments. AssertArgs outputs an error to "stderr"
 * and aborts.
 * ARGS_ASSERT macro is always defined, ASSERTARGS macro is used to trap any
 * NULL pointers as function arguments.
 *
 ****************************************************************************/
#define ARGS_ASSERT

#ifdef ARGS_ASSERT

void AssertArgs (const char *func, char *file, int line);

#define ASSERTARGS(f)					\
  if (f)						\
    {}							\
  else							\
    AssertArgs (__func__, __FILE__,  (int)__LINE__)

#else

#define ASSERTARGS(f)

#endif   /* #ifdef ARGS_ASSERT */


char *myStrdup(const char *src, char *filename, unsigned uLine);

#define STRDUP(x) myStrdup(x, __FILE__, __LINE__)

int isGoodEntry(const char *entry);

int isGoodFilename(const char *name);

int isGoodDirName(const char *path);

int isGoodPathPart(const char *part);

char *getParentDir(char const *path);

char* lastOfPath(const char *path);

int doDummyDir(const char *parent);

int isDirUsable(const char* const path);

int isFileUsable(char const *name);

int isFileReadable(const char *path2File);

int isExecutableUsable(const char *name);

int getFileSize(long *size, const char *filename);

char* getHome();

char *getUserName();

int hasPath(const char *name);

char *prependCWD(const char *name);

char *prependParent(const char *name);

int appendEntry2Path(char **dest, char const *path, char const *name);

char *appendName2Dir(const char *dir, const char *name);




int removeSpaces(char **str);

char  *removeSpaces2(char *str);

int isGoodPortString (const char *port);

int isOkayFormat4URL(char const *url);

char* dropExtension(char *str);

int IsEntryDir (char const *entry);

int isPathDirectory(const char* const path);

int isRegularFile(const char *path);

//char* DropExtension(char *str);

void** allocate2Dim (int row, int col, size_t elemSize);

void free2Dim (void **array, size_t elemSize);



char* get_self_executable_directory ();


int myMkDir (char *name);

int isStrGoodDouble(char *str);



int mySpawn (char *prgName, char **argLst);

int spawnWait (char *prog, char **argsList);

int myGetDirDL (DLIST *dstDL, char *dir);

//void zapString(void **data); 12/18/2023

char* getFormatTime (void);

int string2Upper (char **dst, const char *str);

int string2Lower (char **dest, const char *str);

int hasUpper(const char *str);

int hasLower(const char *str);

int isGoodURL (const char *str);

int getDirList (DLIST *list, const char *dir);

int isStringInList (DLIST *list, const char *str);

int deffList1Not2 (DLIST *dst, DLIST *list1, DLIST *list2);

int convDouble2Long (long *dstL, double srcDouble);

char	 *formatC_Time(void);

char* formatMsgHeadTime(void);

int printBold (char *str);

int mkOutputFile (char **dest, char *givenName, char *rootDir);

FILE* openOutputFile (char *filename);

int closeFile(FILE *fPtr);

int isGoodExecutable(char *file);

char *arg2FullPath(const char *arg);


#endif /* UTIL_H_ */
