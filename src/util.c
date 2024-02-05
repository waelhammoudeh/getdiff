/*
 * util.c
 *
 *  Created on: Jun 23, 2017
 *      Author: wael
 *  small functions ... system calls ... strings functions
 *  I use LINUX system, some of the functions here use Linux system calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>   /* gettimeofday() */
#include <ctype.h> //toupper()
#include <sys/wait.h>

#include "util.h"
#include "ztError.h"


/* function source was: WRITING SOLID CODE by Steve Maguire */
void AssertArgs (const char *func, char *file, int line){

  fflush(NULL);

  fprintf (stderr, "\nAssertArgs: Assertion failed! "
	   "Function: <%s> : File: %s : Line: %d\n", func, file, line);

  fflush(stderr);
  abort();

}  /* END AssertArgs()  */

/* safer strdup() function; on failure prints error message with file name and
 * line number of its caller, and calls abort().
 * Use the defined macro STRDUP(x) to get correct file name and line number.
 *
 **************************************************************/
char *myStrdup(const char *src, char *filename, unsigned uLine){

  char   *duplicate;

  ASSERTARGS(src && filename);

  duplicate = strdup(src);

  if( ! duplicate ){
    fflush(NULL);
    fprintf(stderr, "myStrdup(): Error failed strdup() in file: <%s> at line number: <%d> ... aborting!\n",
	    filename, uLine);
    fflush(stderr);
    abort();
  }

  return duplicate;

} /* END myStrdup() **/

/* isGoodPathPart(): is 'part' a good filename?
 * man pathchk program from core utilities.
 * see isGoodFilename() below.
 *
 ***********************************************/
int isGoodPathPart(const char *part){

  char   *allowed =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_.";

  char   dash = '-';
  char   underscore = '_';
  char   period = '.';

  if(strlen(part) > FNAME_MAX)

    return ztFnameLong;

  if(strspn(part, allowed) != strlen(part))

    return ztFnameDisallowed;

  if(strchr(part, dash) && (part[0] == dash || part[strlen(part) - 1] == dash))

    return ztFnameHyphen; /* miss placed dash **/

  if(strchr(part, underscore) &&
     (part[0] == underscore|| part[strlen(part) - 1] == underscore))

    return ztFnameUnderscore;

  /* no relative paths are allowed including single and double dots. **/
  if(strchr(part, period) && (strlen(part) == 2) &&
     ((part[0] == period) && (part[1] == period)) )

    return ztNoRelativePath;

  if(strchr(part, period) && (strlen(part) == 1))

    return ztNoRelativePath;

  /* period can not be the last character **/
  if(strchr(part, period) && (strlen(part) > 1) && (part[strlen(part) - 1] == period))

    return ztFnamePeriod;

  return ztSuccess;

} /* END isGoodPathPart() **/

/* isGoodFilename(): is good PATH + FILENAME.
 * name must start with slash since it is a path; we do not do path expansion
 * or substitution. Note that we get absolute path from the command line; because
 * the shell does path expansion and substitution for us.
 *
 * allowed set: [alphabets (upper & lower) plus / -_. ]
 *  - the slash character is only delimiter between consecutive entries
 *    it is an error for path to have multiple consecutive slashes.
 *  - hyphen & underscore can not be the first or the last character.
 *  - period can not be the last character.
 *
 *  - maximum filename length is defined by FNAME_MAX = 255
 *    name is assumed to be path + filename,
 *  - maximum string length for path is PATH_MAX,
 *    [PATH_MAX == 4096] in GNU C Library.
 *
 * NOTE1: any path part can be a real or link to directory / filename.
 * NOTE2: to check just the filename by itself use 'isGoodPathPart()'.
 *
 *******************************************************/
int isGoodFilename(const char *name){

  char    slash = '/';
  char    period = '.';
  char    *hasSlash;
  char    *tmpBuf = NULL;
  char    *mover;

  ASSERTARGS(name);

  if(strlen(name) > PATH_MAX)

    return ztFnameLong;

  if( ! strchr(name, slash) )

    return ztStrNotPath;

  if (name[0] == period) /* allow period to be first character **/

    name++;

  if(name[0] != slash)

    return ztStrNotPath;

  if(name[strlen(name) - 1] == slash)

    return ztFnameSlashEnd;

  tmpBuf = STRDUP(name);

  hasSlash = strchr(tmpBuf, slash);

  if ( ! hasSlash )

    return isGoodPathPart(tmpBuf);

  /* check for double - multiple slashes **/

  while(hasSlash){

    mover = hasSlash + 1;

    if(mover[0] == slash)

      return ztFnameMultiSlashes;

    hasSlash = strchr(mover, slash);


  }

  /* check each part **/
  char  *part;
  int   result;

  part = strtok (tmpBuf, "/");

  while(part){

    result = isGoodPathPart(part);

    if(result != ztSuccess)

      return result;

    part = strtok(NULL, "/");

  }

  return ztSuccess;

} /* END isGoodFilename() **/

/* isGoodDirName():
 *
 *
 * return result of isGoodFilename()
 *
 *
 *
 *******************************************************/
int isGoodDirName(const char *path){

  char   *myCopy;
  int    result;

  if( ! path )

    return ztInvalidArg;

  myCopy = STRDUP(path);

  if(SLASH_ENDING(myCopy))

    myCopy[strlen(myCopy) - 1] = '\0';

  if(SLASH_ENDING(myCopy)){

    free(myCopy);
    return ztFnameMultiSlashes;
  }

  /* we still need slash to make good directory name **/
  if( ! strchr(myCopy, '/') ){

    free(myCopy);
    return ztStrNotPath;
  }

  result = isGoodFilename(myCopy);

  free(myCopy);

  return result;

} /* END isGoodDirName() **/

/* getParentDir(): function returns a pointer to parent directory of its argument.
 * parent directory is the path just before last slash.
 *
 * function calls abort() if its 'path' argument is NULL.
 *
 * function returns NULL on error as follows:
 *   - if string length of 'path' is more than PATH_MAX
 *   - if 'path' has no slash
 *   - if 'path' is only one single character -- including root directory only
 *   - if its argument 'path' is not a good filename or directory name
 *
 ****************************************************************/

char *getParentDir(char const *path){

  char   *retCh = NULL;
  char   *lastSlash;
  char   tempBuf[PATH_MAX] = {0};
  char   slash = '/';

  ASSERTARGS(path);

  if(strlen(path) > PATH_MAX)

    return retCh;

  /* get our own copy **/
  strcpy(tempBuf, path);

  /* if it ends with a slash, remove it **/
  if(SLASH_ENDING(tempBuf))

    tempBuf[strlen(tempBuf) - 1] = '\0';

  if (isGoodFilename(tempBuf) != ztSuccess)

    return retCh;

  lastSlash = strrchr(tempBuf, slash);

  lastSlash[0] = '\0';

  retCh = STRDUP(tempBuf);

  return retCh;

} /* END getParentDir() */

/* lastOfPath() : Function allocates memory for last entry in its 'path' parameter,
 * copies it and return a pointer for the copied string. Function returns empty
 * string if 'path' has only the slash character.
 *
 * Function aborts program if 'path' parameter is NULL.
 *
 * Function returns NULL on error as follows:
 *
 *  - if string length of 'path' parameter is > PATH_MAX.
 *  - if 'path' parameter is empty string - its string length == zero.
 *
 * ***************************************************************** */

char* lastOfPath (const char *path){

  char  *pointer = NULL;

  char  slash = '/';
  char  *lastSlash = NULL;
  char  myPath[PATH_MAX] = {0};

  ASSERTARGS(path);

  if ( (strlen(path) > (PATH_MAX)) || (strlen(path) == 0) )

    return pointer;

  /* get our own copy **/
  strcpy(myPath, path);

  /* if it ends with slash; remove it - while() is probably wrong! **/
  while(SLASH_ENDING(myPath))

    myPath[strlen(myPath) - 1] = '\0';


  if(! strchr(myPath, slash)){

	pointer = STRDUP(myPath);

    return pointer;
  }

  lastSlash = strrchr(myPath, slash);

  lastSlash++;

  pointer = STRDUP(lastSlash);

  return pointer;

} /* END lastOfPath() **/

/* doDummyDir(): function creates then removes a directory under specified parent
 *
 * returns: ztSuccess or ztFailedSysCall on error.
 *
 *******************************************************************/
int doDummyDir(const char *parent){

  char   *dummyName = "getdiff_dummy";
  char   myDir[PATH_MAX + 1] = {0};
  int    result;

  ASSERTARGS(parent);

  if(SLASH_ENDING(parent))
    sprintf(myDir, "%s%s", parent, dummyName);
  else
    sprintf(myDir, "%s/%s", parent, dummyName);

  errno = 0;
  result = mkdir (myDir, MK_DIR_MODE);

  if (result !=  0){
    perror("Failed mkdir system call:");
    return ztFailedSysCall;
  }

  /* sleep(1); **/

  result = rmdir(myDir);
  if (result !=  0){
    perror("Failed rmdir system call:");
    return ztFailedSysCall;
  }

  return ztSuccess;

} /* END doDummyDir() **/

/* isDirUsable():
 * return ztSuccess when it is usable
 */
int isDirUsable(const char* const path){

  int    result;
  struct stat status;

  ASSERTARGS(path);

  /* path must be good directory name **/
  result = isGoodDirName(path);
  if(result != ztSuccess)

    return result;

  errno = 0;

  /* fill struct stat with lstat system call ENOENT ENOTDIR **/
  result = stat(path, &status);
  if(result != 0){

    if((errno == ENOENT) || (errno == ENOTDIR))

      return ztPathNotDir;

    else if(errno){

      fprintf(stderr, "Failed System call to stat(): %s\n", strerror(errno));
      return ztFailedSysCall;
    }
  }

  if ( ! (S_ISDIR (status.st_mode)) )

    return ztPathNotDir;

  /* if ( ! (status.st_mode & S_IRUSR)) -- avoid for
   *  THIS IS OWNING USER NOT effective / current user
   *********************************************/

  /* can we access it? we need READ, WRITE and EXECUTE permissions **/
  if (access(path, R_OK | W_OK | X_OK) != 0)

    return ztInaccessibleDir;

  /* run test function: makes & removes a directory
   *  doDummyDir() appends a new entry to 'path'
   *  temporary directory is created under 'path' then removed.  **/

  result = doDummyDir(path);
  if(result != ztSuccess)

    return result;


  return ztSuccess;

} /* END isDirUsable() **/

/* isFileUsable(): are we able to change file? **/

int isFileUsable(const char *path2File){

  int    result;
  char   *parent;

  struct stat status;

  ASSERTARGS(path2File);

  /* path2File must be good filename **/
  result = isGoodFilename(path2File);
  if(result != ztSuccess)

    return result;

  /* we need full access to its parent directory **/
  parent = getParentDir(path2File);
  if( ! parent )

    return ztMemoryAllocate; /* most likely reason since filename was okay **/

  result = isDirUsable(parent);
  if (result != ztSuccess)

    return result;

  /* set errno & call lstat() **/

  errno = 0;
  result = stat(path2File, &status);
  if(result == -1){

    if(errno)

      return ztFileNotFound;

  }

  if ( ! S_ISREG (status.st_mode))

    return ztNotRegFile;

  if (status.st_size == 0)

    return ztFileEmpty;

  /* need to be able to read and write it **/
  if (access(path2File, R_OK | W_OK) != 0)

    return ztInaccessibleFile;

  return ztSuccess;

} /* END isFileUsable() **/

/* isFileReadable(): can we read file? **/
int isFileReadable(const char *path2File){

  int    result;
  //  char   *parent;

  struct stat status;

  ASSERTARGS(path2File);

  /* path2File must be good filename **/
  result = isGoodFilename(path2File);
  if(result != ztSuccess)

    return result;

  errno = 0;
  result = stat(path2File, &status);
  if(result == -1){

    if(errno == ENOENT)

      return ztFileNotFound;

    else

      return ztFailedSysCall;
  }

  if ( ! S_ISREG (status.st_mode))

    return ztNotRegFile;

  if (status.st_size == 0)

    return ztFileEmpty;

  /* need to be able to read and write it **/
  if (access(path2File, R_OK) != 0)

    return ztInaccessibleFile;

  return ztSuccess;

} /* END isFileReadable() **/

int isExecutableUsable(const char *name){

  int    result;

  struct stat status;

  ASSERTARGS(name);

  result = isGoodFilename(name);
  if(result != ztSuccess)

    return result;

  errno = 0;
  result = stat(name, &status);
  if(result == -1){

    if(errno == ENOENT)

      return ztFileNotFound;

    else

      return ztFailedSysCall;
  }

  if( ! (status.st_mode & S_IXOTH) ) //S_IXOTH is executable for other group

    return ztNotExecutableFile;

  /*
    if (access(name, X_OK) != 0)

    return ztNotExecutableFile;

    ** commented out, same as above **/


  return ztSuccess;

} /* END isExecutableUsable() **/

int getFileSize(long *size, const char *filename){

  int    result;
  struct stat status;

  ASSERTARGS(filename && size);

  *size = 0; /* set size to zero **/

  result = isGoodFilename(filename);
  if(result != ztSuccess)

    return result;

  errno = 0;
  result = stat(filename, &status);
  if(result == -1){

    if(errno == ENOENT)

      return ztFileNotFound;

    else

      return ztFailedSysCall;
  }

  /* ensure it is a regular file **/
  if(! S_ISREG(status.st_mode))

    return ztNotRegFile;

  *size = (long) status.st_size;

  return ztSuccess;

} /* END getFileSize() **/

/* getHome(): function gets home directory for effective user.
 *
 * The simple method is to pull the environment variable "HOME"
 * The slightly more complex method is to read it from the system user database.
 *
 * Note: function may return NULL ... ALWAYS check return.
 *
 * FIXME: use errno for getuid() and getpwuid() system functions.
 *
 ************************************************************************/

char* getHome(){

  char *resultDir = NULL;

  char *homeDir = getenv("HOME");

  if (homeDir && (isDirUsable(homeDir) == ztSuccess) ){

    resultDir = STRDUP(homeDir);
    return resultDir;
  }

  uid_t uid = getuid();
  struct passwd *pw = getpwuid(uid);

  if ( ! (pw && pw->pw_dir) )

    return NULL;

  homeDir = pw->pw_dir;

  if (homeDir && (isDirUsable(homeDir) == ztSuccess) )

    resultDir = STRDUP(homeDir);

  return resultDir;

} /* END getHome() **/

/* getUserName(): returns effective user name, abort()s if unsuccessful */

char *getUserName(){

  char *user = NULL;

  user = getenv("USER");

  if (user)

    return user;

  uid_t uid = getuid();
  struct passwd *pw = getpwuid(uid);

  if ( ! (pw && pw->pw_dir) )

    abort();

  user = pw->pw_name;

  if ( ! user )

    abort();

  return user;

} /* END getUser() **/

/* isCwdChild(): is current working directory child
 *
 * we refer to files or directories in CWD by:
 *  - filename or ./filename
 *    and for directory:
 *  - dirName or ./dirName
 *
 * child of CWD does not start with slash or starts with "./"
 *
 **************************************************/

int isCwdChild(const char *name){

  char   *cwdStr = "./";
  char   slash = '/';

  ASSERTARGS(name);

  if(name[0] != slash)

    return TRUE;

  if(strstr(name, cwdStr) && (strstr(name, cwdStr) == name) )

    return TRUE;

  return FALSE;

}

/* prependCWD(): parameter name may start with ./
 *
 **********************************************/
char *prependCWD(const char *name){

  static char  *path = NULL;
  char  buffer[PATH_MAX] = {0};
  char  cwd[PATH_MAX - 512] = {0};
  char  *gtResult;

  ASSERTARGS(name);

  if(strstr(name, "./") == name)

    name = name + 2;


  gtResult = getcwd(cwd, sizeof(cwd));
  if(!gtResult)

    return path;


  if(SLASH_ENDING(cwd))
    sprintf(buffer, "%s%s", cwd, name);
  else
    sprintf(buffer, "%s/%s", cwd, name);

  path = STRDUP(buffer);

  return path;

} /* END prependCWD() **/

int hasPath(const char *name){

  ASSERTARGS(name);

  if((name[0] == '/') && strchr(name+1, '/'))

    return TRUE;

  return FALSE;

} /* END hasPath() **/

/* prependParent():
 * prepend parent directory only with string starting "../"
 *
 ********************************************************/

char *prependParent(const char *name){

  static char  *path = NULL;
  char  buffer[PATH_MAX] = {0};
  char  cwd[PATH_MAX - 512] = {0};
  char  *gwdResult;
  char  *parent;

  char  *foundChar;

  ASSERTARGS(name);

  foundChar = strstr(name, "../");

  if(! foundChar)

    return path;

  if(foundChar != name)

    return path;

  name = name + strlen("../");

  gwdResult = getcwd(cwd, sizeof(cwd));
  if(!gwdResult)

    return path;

  parent = getParentDir(cwd);
  if(!parent)

    return path;

  if(SLASH_ENDING(parent))
    sprintf(buffer, "%s%s", parent, name);
  else
    sprintf(buffer, "%s/%s", parent, name);

  path = STRDUP(buffer);

  return path;

} /* prependParent() **/

/* removeSpaces(): removes leading and trailing space from its argument.
 *
 * NOTE: the argument 'str' is a POINTER to POINTER. str should be dynamically
 * allocated to have it cleaned in place, call removeSpaces2() below if this is not
 * the case with your string.
 *********************************************************************/
int removeSpaces(char **str){

  char    *originalPtr;
  char    *cleanStr;
  char    *end;

  ASSERTARGS(str && *str);

  originalPtr = *str;
  cleanStr = *str;


  /* remove leading spaces **/
  while (isspace( (unsigned char) *cleanStr))

    cleanStr++;

  if(cleanStr != originalPtr){

    char *tempStr;

    tempStr = STRDUP(cleanStr);

    memset(originalPtr, 0, strlen(originalPtr));

    strcpy(originalPtr, tempStr);
    cleanStr = originalPtr;
  }

  if(cleanStr[0] == 0)

    return ztSuccess;

  /* remove trailing space - let 'end' point at last character **/
  end = cleanStr + strlen(cleanStr) - 1;
  while(end > cleanStr && isspace((unsigned char) *end))

    end--;

  /* terminate the string AFTER end. (*end == end[0]) **/
  end[1] = '\0';

  /* set caller pointer **/
  *str = cleanStr;

  return ztSuccess;

} /* END removeSpaces() **/

/* removeSpaces(): function removes leading and trailing spaces from its argument.
 * Spaces are as defined by 'isspace() : space, form feed, newline, carriage return,
 * vertical tab and horizontal tab.
 *
 * Function allocates memory for new clean string and returns pointer to it. Returns
 * NULL on memory allocation failure.
 *
 * NOTE: argument 'str' does NOT need to be dynamically allocated.
 *
 *************************************************************************/

char  *removeSpaces2(char *str){

  char    *end;
  char    *myStr;

  ASSERTARGS(str);

  /* get our own copy of string **/
  myStr = strdup(str);
  if ( ! myStr )

    return NULL;

  /* remove leading space **/
  while (isspace( (unsigned char) *myStr))

    myStr++;

  if(myStr[0] == 0)  /* all spaces? **/

    return myStr;

  /* remove trailing space **/
  end = myStr + strlen(myStr) - 1;
  while(end > myStr && isspace((unsigned char) *end))

    end--;

  /* terminate the string AFTER end. (*end == end[0]) **/
  end[1] = '\0';

  return myStr;

} /* END removeSpaces2(char *str) **/

int isGoodPortString (const char *port){

  long    num;
  char    *end;

  uint MAX_PORT = 65535;

  ASSERTARGS(port);

  num = strtol (port, &end, 10);

  if (*end != '\0') /* non-digit in string port **/

    return ztInvalidArg;

  if (num < 1 || num > MAX_PORT)

    return ztInvalidArg;

  return ztSuccess;

} /* END isGoodPortString() **/

/* isOkayFormat4HTTPS(): is the URL string pointed to by 'source' argument
 * in good format for "HTTPS" protocol (scheme) URL?
 *   - string must start with 'https://'
 *   - the rest of the string with last slash from 'https://' included; must be
 *     good path name; that is "goodDirName()".
 *
 * returns TRUE for good format or FALSE for bad format.
 *
 *****************************************************************************/
int isOkayFormat4HTTPS(char const *source){

  char   *mySource;
  char   *protocol = "https://";
  char   *head, *tail;
  int    result;


  ASSERTARGS(source);

  mySource = STRDUP(source);

  removeSpaces(&mySource);

  head = strstr(mySource, protocol);

  if(! (head && (head == mySource)) )

    return FALSE;

  tail = mySource + strlen(protocol) - 1; /* path MUST start with a slash **/

  result = isGoodDirName(tail);
  if(result != ztSuccess){
    fprintf(stderr, "isOkayFormat4HTTPS: Error source string failed isGoodDirName() function.\n");
    return FALSE;
  }

  return TRUE;

} /* END isOkayFormat4HTTPS() **/


/* isGoodURL():
 *   - can contain any of declared "allowed" character set below.
 *   - does NOT contain any of those characters between < and > below:
 *   < []{}<> !@#$%^&*()+=,;:'"|` \040\t >
 *   - string length must be less than PATH_MAX
 *   - an entry can not start with dash '-' or underscore '_'
 *
 *   * added on December 9/2023 **
 *   - string must include at lease 3 parts separated by delimiter '/'
 *     three parts for (scheme + server_name + path)
 *
 * Return TRUE or FALSE.
 * NOTE: May return FALSE on memory allocation error.
 *
 */

int isGoodURL (const char *str){


  char *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789:./-_~";
  char dash = '-';
  char underscore = '_';
  char *myStr;
  int hasDisallowed;
  char *del = "/";
  char *token;

  if ( str == NULL )

    return FALSE;

  if (strlen (str) > PATH_MAX)

    return FALSE;

  hasDisallowed = ( strspn(str, allowed) != strlen(str) );
  if (hasDisallowed)

    return FALSE;

  myStr = strdup(str);
  if ( ! myStr ){
    fprintf(stderr, "isGoodURL(): error memory allocation; failed strdup() call.\n");
    //abort();
    return FALSE;
  }

  /* entry can not start or end with dash or underscore character **/

  int count = 0; /* added 12/9/2023 to count parts **/

  token = strtok (myStr, del);
  while (token){

    count++; /* count tokens **/

    if(strchr(token, dash) &&
       (token[0] == dash || token[strlen(token) - 1] == dash))

      return FALSE;

    if(strchr(token, underscore) &&
       (token[0] == underscore || token[strlen(token) - 1] == underscore))

      return FALSE;

    token = strtok (NULL, del);
  }

  /* added 12/9/2023 : require at least 3 tokens
   * assumed for (scheme + server_name + path)
   * none of them should empty **/
  if(count < 3){
    fprintf(stderr, "isGoodURL(): Error parameter token count < 3\n"
	    "  string should include (scheme + server_name + path) at least.\n");
    return FALSE;
  }

  return TRUE;

} /* END isGoodURL() **/




/* DropExtension(): drops extension from str in; anything after a right period.
 * allocates memory for return pointer. returns NULL if str > PATH_MAX or
 * memory allocation error. you get a duplicate if str has no period.
 **************************************************************************/
char* dropExtension(char *str){

  char *retCh = NULL;
  int   period = '.';
  char *periodCh;
  char  temp[PATH_MAX] = {0};

  ASSERTARGS(str);

  if(strlen(str) > PATH_MAX)

    return retCh;

  periodCh = strrchr(str, period);

  if(periodCh)

    strncpy(temp, str, strlen(str) - strlen(periodCh));

  else

    strcpy(temp, str);

  retCh = (char *)malloc(sizeof(char) * (strlen(temp) + 1));
  if(! retCh)

    return retCh;

  strcpy(retCh, temp);

  return retCh;

}


/* allocate2x2(): allocates memory for 2 dimensional array,
 * array is with dimension row x col, and element size is elemSize
 * allocates one extra element and sets it to NULL.
 * all elements are initialed empty.
 * row, col and elemSize must be > 0 */

void** allocate2Dim (int row, int col, size_t elemSize){

  void **array = NULL;
  void  **mover;
  int  iRow, jCol;

  if(row < 1 || col < 1 || elemSize < 1){
    printf("allocate2Dim(): ERROR at least one parameter is less than 1\n"
	   "returning NULL\n");
    return array;
  }

  array = (void**) malloc(sizeof(void*) * (row * col) + 1);
  if(array == NULL)
    return array;

  mover = array;
  for (iRow = 0; iRow < row; iRow++){
    for (jCol = 0; jCol < col; jCol++){
      *mover = (void*) malloc (elemSize);
      if (*mover == NULL){
	array = NULL;
	return array;
      }
      memset(*mover, 0, elemSize);
      mover++;
    }
  }
  *mover = NULL;  /* set last one to NULL. */

  //printf("allocate2x2(): returning array :: bottom of function\n\n");

  return array;
}

/* free2Dim(): call only to free allocate2Dim() */
void free2Dim (void **array, size_t elemSize){

  void **mover;

  ASSERTARGS (array); // abort if not array!!

  mover = array;

  while(*mover){

    memset (*mover, 0, elemSize);
    free(*mover);
    mover++;
  }

  free(array);

} // END free2Dim()


/* from Advanced-Linux Programming --- still needs work */
char* get_self_executable_directory (){

  int  rval;
  char  link_target[1024];
  char *last_slash;
  size_t  result_length;
  char *result;

  /* Read the target of the symbolic link /proc/self/exe. */
  rval = readlink ("/proc/self/exe", link_target, sizeof (link_target));
  if (rval == -1)
    /* The call to readlink failed, so bail. */
    return NULL; //abort ();
  else
    /* NULL-terminate the target. */
    link_target[rval] = '\0';

  /* We want to trim the name of the executable file, to obtain the
     directory that contains it. Find the rightmost slash. */
  last_slash = strrchr (link_target, '/');
  if (last_slash == NULL || last_slash == link_target)
    /* Something strange is going on. */
    abort ();

  /* Allocate a buffer to hold the resulting path. */
  result_length = last_slash - link_target;
  result = (char*) malloc (result_length + 1);
  if(result == NULL){

    return NULL;
  }

  /* Copy the result. */
  strncpy (result, link_target, result_length);
  result[result_length] = '\0';

  return result;

}



/* MyMkDir(): function to create the argument specified directory using the
 * permission defined (in util.h), if the directory exist this function will
 * return WITHOUT error. It is NOT an error if it exist. Function will NOT
 * make parent.
 */
int myMkDir (char *name){

  int result;

  errno = 0;
  result = mkdir (name, MK_DIR_MODE);

  if ( (result ==  -1) && (errno == EEXIST) )

    return ztSuccess;

  else if (result == -1){

    fprintf (stderr, "MyMkDir(): Error mkdir <%s>, system error says: %s\n",
	     name, strerror(errno));

    return ztFailedSysCall;
  }

  return ztSuccess;

}   /* END MyMkDir()  */

/* is string good double string? all digits, plus, minus and period */
int isStrGoodDouble(char *str){

  char *allowed = "-+.0123456789";

  if (strspn(str, allowed) != strlen(str))

    return FALSE;

  return TRUE;
}


int mySpawn (char *prgName, char **argLst){

  pid_t childPID;

  ASSERTARGS(prgName && argLst);

  childPID = fork();

  if (childPID == 0){   /* this is the child process */
    /* prototype from man page:
     * int execv(const char *path, char *const argv[]); */

    execv (prgName, argLst);

    /* The execv function returns only if an error occurs. */
    printf ("mySpawn(): I am the CHILD!!!\n"
	    "If you see this then there was an error in execv\n");
    fprintf (stderr, "mySpawn(): an error occurred in execv\n");
    abort();
  }
  else

    return childPID;
}

int spawnWait (char *prog, char **argsList){

  /* wait() : caller (parent) waits for child process to finish */
  //pid_t childPid;
  int childStatus;

  // don't allow nulls
  ASSERTARGS(prog && argsList);

  // ignore child PID returned by mySpawn()
  mySpawn(prog, argsList);

  // wait for the child to finish
  wait(&childStatus);

  //if (WIFEXITED(childStatus)) ??? normal exit
  if (WEXITSTATUS(childStatus) == EXIT_SUCCESS)

    return ztSuccess;

  else {

    fprintf(stderr, "spawnWait(): Error child process exited abnormally! With exit code: %d\n",
	    WEXITSTATUS(childStatus));
    // TODO: maybe get system error; careful what you wish for, is that a system error?
    return ztChildProcessFailed;
  }

} /* END "spawnWait() **/





/* IsEntryDir(): function to test if the specified entry is a directory or not.
 * Argument: entry : entry to test for.
 * Return: TRUE if entry is a directory, FALSE otherwise.
 *
 */

int IsEntryDir (char const *entry) {

  struct stat status;

  if (entry == NULL)
    return FALSE;

  errno = 0;

  if (lstat (entry, &status) !=  0){
    /* fill status structure, lstat returns zero on success */
    fprintf (stderr, "IsEntryDir(): Could NOT lstat entry:  %s . "
	     "System says: %s\n",
	     entry, strerror(errno));
    return FALSE;
  }

  if (S_ISDIR (status.st_mode))

    return TRUE;

  return FALSE;

}  /* END IsEntryDir()  */

/* isPathDirectory() function to replace old IsEntryDir()
 *
 */
int isPathDirectory(const char* const path){

  struct stat status;
  int    result;

  ASSERTARGS(path);

  errno = 0;

  result = lstat(path, &status);
  if(result != 0){

    /* return false regardless of the error **/
    perror("stat() system call failure:");
    return FALSE;
  }

  if (S_ISDIR (status.st_mode))

    return TRUE;

  return FALSE;

} /* END isDirectory() **/

int isRegularFile(const char *path){

  struct stat status;
  int    result;

  ASSERTARGS(path);

  errno = 0;

  result = lstat(path, &status);
  if (result != 0) {
    /* return false regardless of the error **/
    perror("lstat() system call failure:");
    return FALSE;
  }

  if (S_ISREG(status.st_mode)) {
    return TRUE;
  }

  return FALSE;

} /* END isRegularFile() **/


/* myGetDirDL() function to read directory and place entries in sorted list.
 * The returned list WILL INCLUDE the full entry path.
 * This function will NOT include the dot OR double dot entries.
 * caller allocates and initials dstDL.
 * checked run time error for dstDL to be empty and for dir to point to a
 * directory and be accessible.
 * **************************************************************************/

int myGetDirDL (DLIST *dstDL, char *dir){

  DIR  *dirPtr;
  struct  dirent  *entry;
  char  dirPath[PATH_MAX + 1];
  char tempBuf[PATH_MAX + 1];
  char  *fullPath;
  int   result;

  ASSERTARGS (dstDL && dir);

  if (DL_SIZE(dstDL) != 0){
    printf("myGetDirDL(): Error list not empty.\n");
    return ztListNotEmpty;
  }

  if ( ! IsEntryDir(dir)){
    printf ("myGetDirDL(): Error specified argument is not a directory.\n");
    return ztPathNotDir;
  }

  if (isDirUsable(dir) != ztSuccess){
    printf ("myGetDirDL(): Error specified directory not usable.\n");
    return ztInaccessibleDir;
  }

  errno = 0;
  dirPtr = opendir (dir);
  if (dirPtr == NULL){   /* tell user why it failed */
    printf ("myGetDirDL(): Error opening directory %s, system says: %s\n",
	    dir, strerror(errno));
    return ztFailedSysCall;
  }

  /* append a slash if it is not there. */
  if ( dir[ strlen(dir) - 1 ] == '/' )
    sprintf (dirPath, "%s", dir);
  else
    sprintf (dirPath, "%s/", dir);

  while ( (entry = readdir (dirPtr)) ){

    if ( (strcmp(entry->d_name, ".") == 0) ||
	 (strcmp(entry->d_name, "..") == 0) )

      continue;

    ASSERTARGS (snprintf (tempBuf, PATH_MAX, "%s%s",
			  dirPath, entry->d_name) < PATH_MAX);

    fullPath = (char *)malloc(strlen(tempBuf) + 1);
    if( ! fullPath){
      printf("myGetDirDL(): error allocating memory.\n");
      return ztMemoryAllocate;
    }
    strcpy(fullPath, tempBuf);
    result = ListInsertInOrder (dstDL, fullPath);
    if (result != ztSuccess){
      printf("myGetDirDL(): Error returned by ListInsertInOrder().\n");
      printf(" Message: %s\n\n", ztCode2Msg(result));
      return result;
    }
  }

  closedir(dirPtr);

  return ztSuccess;

}  /* END myGetDirDL()  */

/** exist in "primitives.c"

    void zapString(void **string){

    if((char *) *string){
    free(*string);
    *string = NULL;
    }

    return;

    }

**/

/* GetFormatTime(): formats current time in a string buffer, allocates buffer
 * and return buffer address.  */

char* getFormatTime (void){

  char *ret = NULL;
  char buffer[1024];
  char   timeBuf[80];
  long    milliSeconds;
  struct    timeval  startTV;   /* timeval has two fields: tv_sec: seconds  and tv_usec: MICROseconds */
  struct    tm *timePtr;

  gettimeofday (&startTV, NULL);
  timePtr = localtime (&startTV.tv_sec);
  strftime (timeBuf, 80, "%a, %b %d, %Y %I:%M:%S %p", timePtr);
  milliSeconds = startTV.tv_usec / 1000;

  sprintf (buffer, "%s :milliseconds: %03ld", timeBuf, milliSeconds);

  ret = (char *) malloc ((strlen(buffer) + 1) * sizeof(char));
  if (ret == NULL){
    printf ("getFormatTime(): Error allocating memory.\n");
    return ret;
  }

  strcpy (ret, buffer);

  return ret;

} /* END GetFormatTime() */

char  *formatC_Time(void){

  char *ret = NULL;
  char   tmpBuf[80];

  struct tm *timePtr;
  time_t timeValue;

  time(&timeValue);
  timePtr = localtime(&timeValue);

  /* %c is current locale time format for strftime() **/
  strftime(tmpBuf, 80, "%c", timePtr);

  ret = (char *) malloc ((strlen(tmpBuf) + 1) * sizeof(char));
  if (ret == NULL){
    fprintf (stderr, "formatC_Time(): Error allocating memory.\n");
    return ret;
  }

  strcpy (ret, tmpBuf);

  return ret;
}

/* formatMsgHeadTime(): compact time format used as line beginning of
 * logged message : 2022-Mar-21 19:45:37 note no line feed.
 *
 * Head -> head of the line!
 * ***********************************************************************/
char* formatMsgHeadTime(void){

  char *ret = NULL;
  char   tmpBuf[80];

  struct tm *timePtr;
  time_t timeValue;

  time(&timeValue);
  timePtr = localtime(&timeValue);

  strftime(tmpBuf, 80, "%Y-%b-%d %H:%M:%S", timePtr);

  ret = (char *) malloc ((strlen(tmpBuf) + 1) * sizeof(char));
  if (ret == NULL){
    fprintf (stderr, "formatC_Time(): Error allocating memory.\n");
    return ret;
  }

  strcpy (ret, tmpBuf);

  return ret;

}

/****************************** StringToLower() ******************************
stringToLower: Function to convert a string to lower character case.
Argument: dest is a pointer to pointer to character for destination string.
          str is a pointer to source string.
Return: ztSuccess on success. ztMemoryAllocate on memory failure or ztInvalidArg
when source string length is zero.
This function allocates memory for destination. Error is returned on memory
failure.
******************************************************************************/

int stringToLower (char **dest, char *str){

  char  *mover;

  ASSERTARGS (dest && str);

  if (strlen(str) == 0){
    printf("stringToLower(): Error empty str argument! Length of zero.\n");
    return ztInvalidArg;
  }

  *dest = (char *) malloc (sizeof(char) * (strlen(str) + 1));
  if (dest == NULL){
    printf("stringToLower(): Error allocating memory.\n");
    return ztMemoryAllocate;
  }

  strcpy (*dest, str);

  mover = *dest;

  while ( *mover ){
    *mover = tolower(*mover);
    mover++;
  }

  return ztSuccess;

}  /* END StringToLower() */

int stringToUpper (char **dst, char *str){

  char  *mover;

  ASSERTARGS (dst && str);

  if (strlen(str) == 0){
    printf("stringToUpper(): Error empty str argument! Length of zero.\n");
    return ztInvalidArg;
  }

  *dst = (char *) malloc (sizeof(char) * (strlen(str) + 1));
  if (dst == NULL){
    printf ("stringToUpper(): Error, allocating memory.\n");
    return ztMemoryAllocate;
  }

  strcpy (*dst, str);

  mover = *dst;

  while ( *mover ){
    if (*mover > 127){
      printf ("stringToUpper(): Error!! A character is larger than largest ASCII 127 decimal.\n");
      printf ("stringToUpper(): character is: <%c>, ASCII value: <%d>. String is: <%s>\n\n",
	      *mover, *mover, str);
      return ztInvalidArg;
    }
    *mover = toupper(*mover);
    mover++;
  }

  return ztSuccess;

}  /* END stringToUpper() */

/* mkOutFile(): make output file name, sets dest to givenName if it has a slash,
 * else it appends givenName to rootDir and then sets dest to appended string
 */
int mkOutputFile (char **dest, char *givenName, char *rootDir){

  char slash = '/';
  char *hasSlash;
  char tempBuf[PATH_MAX] = {0};

  ASSERTARGS (dest && givenName && rootDir);

  hasSlash = strchr (givenName, slash);

  if (hasSlash)

    *dest = (char *) strdup (givenName); // strdup() can fail .. check it FIXME

  else {

    if(IsSlashEnding(rootDir))

      snprintf (tempBuf, PATH_MAX -1 , "%s%s", rootDir, givenName);
    else
      snprintf (tempBuf, PATH_MAX - 1, "%s/%s", rootDir, givenName);

    *dest = (char *) strdup (&(tempBuf[0]));

  }

  return ztSuccess;
}

/* openOutputFile(): opens filename for writing, filename includes path */

FILE* openOutputFile (char *filename){

  FILE *fPtr = NULL;

  ASSERTARGS (filename);

  errno = 0; //set error number

  //try to open the file for writing
  fPtr = fopen(filename, "w");
  if (fPtr == NULL){

    fprintf (stderr, "openOutputFile(): Error opening file: <%s>\n", filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    perror("The call to fopen() failed!");
  }

  return fPtr;

} // END openOutputFile()

int closeFile(FILE *fPtr){

  int    result;

  ASSERTARGS(fPtr);

  errno = 0; /* set error number */

  result = fclose(fPtr); /* calls fflush() */

  if (result != ztSuccess){

    fprintf (stderr, "closeFile(): Error failed fclose() system call.\n");
    fprintf(stderr, "  System error message: <%s>\n", strerror(errno));
    perror("The call to fclose() failed!");
  }

  return result;

} /* END closeFile() */

/* getDirList() function to read directory and place entries in sorted list.
 * This function will NOT include the dot OR double dot entries. */

int getDirList (DLIST *list, const char *dir){

  DIR *dirPtr;
  struct dirent *entry;
  // char dirPath[PATH_MAX + 1];
  char *name;

  ASSERTARGS (dir && list);

  if ( ! IsEntryDir(dir)){
    fprintf (stderr, "getDirList(): Error specified argument is not a directory.\n");
    return ztInvalidArg;
  }

  /* append a slash if it is not there. */
  /* if (IsSlashEnding(dir))
     sprintf (dirPath, "%s", dir);
     else
     sprintf (dirPath, "%s/", dir);
  */
  errno = 0;
  dirPtr = opendir (dir);
  if (dirPtr == NULL){   /* tell user why it failed */
    fprintf (stderr, "getDirList(): Error opening directory %s, system says: %s\n",
	     dir, strerror(errno));
    return ztFailedSysCall ;
  }

  while ( (entry = readdir (dirPtr)) ){

    if ( (strcmp(entry->d_name, ".") == 0) ||
	 (strcmp(entry->d_name, "..") == 0) )
      continue;

    name = (char *) malloc ((strlen(entry->d_name) + 1) * sizeof(char));
    if ( ! name ){
      fprintf(stderr, "getDirList(): Error allocating memory.\n");
      return ztMemoryAllocate;
    }

    sprintf (name, "%s", entry->d_name);
    ListInsertInOrder (list, name);

  }

  closedir(dirPtr);

  return ztSuccess;

}  /* END getDirList()  */

/* I think there is find string function somewhere - it returns (ELEM*) too I think. **/
int isStringInList (DLIST *list, const char *str){

  ELEM   *elem;
  char   *elemStr;

  ASSERTARGS (list && str);

  if (DL_SIZE(list) < 1)

    return FALSE;

  elem = DL_HEAD(list);

  while(elem){

    elemStr = (char *) DL_DATA(elem);

    if (strcmp(elemStr, str) == 0)

      return TRUE;

    elem = DL_NEXT(elem);

  }

  return FALSE;

}

/* deffList1Not2 (): strings in list1 that are NOT in list2 are included
 * in dst list.
 */

int deffList1Not2 (DLIST *dst, DLIST *list1, DLIST *list2){

  ELEM *elem;
  char *str;
  char *strCpy;


  ASSERTARGS (dst && list1 && list2);

  // ignoring list size today only

  elem = DL_HEAD(list1);
  while(elem){

    str = (char *) DL_DATA(elem);

    if ( ! isStringInList(list2, str)){ // not in list2, we added it to dst

      strCpy = strdup(str);
      if ( ! strCpy)
	return ztMemoryAllocate;

      ListInsertInOrder (dst, strCpy);

    }

    elem = DL_NEXT(elem);
  }

  return ztSuccess;
}

/* NOTE .. NOTE
 * Curl Library uses doubles for sizes sent, received & maybe codes!!!
 * This function is for that ONLY.
 *
 * convDouble2Long(): function converts double parameter srcDouble to
 * long, value is stored in the pointer parameter dstL.
 * srcDouble can NOT have any fraction and is in the range of zero and less
 * than LONG_MAX. On my system with glibc version 2.33 LONG_MAX is defined
 * in stdint.h and limits.h as: LONG_MAX 9223372036854775807L.
 * Caller should guard against negative double.
 * Function returns ztSuccess - zero - on success and errors as follow:
 * ztOutOfRangePara : if srcDouble is negative or larger than LONG_MAX.
 * ztInvalidArg : if srcDouble has fraction.
 *
 *****************************************************************************/

int convDouble2Long (long *dstL, double srcDouble){

  /*
    printf ("LONG_MAX is: %ld\n", LONG_MAX);
    printf ("ULONG_MAX is: %lu\n", ULONG_MAX);
  */

  char buf[512] = {0};
  char *fraction, *whole;
  char *allowed = "0",
    *digits = "1234567890",
    *periodDel = ".",
    *tabDel = "\t",
    *endPtr = NULL;

  long numL;
  int    result;

  ASSERTARGS (dstL);

  if (srcDouble < 0.0) {
    fprintf(stderr, "convDouble2Long(): Error parameter srcDouble is negative\n");
    return ztInvalidArg;
  }

  /* place number in a string with 17 decimal points + tab character */
  sprintf (buf, "%64.17f\t", srcDouble);

  whole = strtok(buf, periodDel);
  fraction = strtok(NULL, tabDel);

  result = removeSpaces(&fraction);
  if(result != ztSuccess){
    fprintf(stderr, "convDouble2Long(): Error failed removeSpaces() function for fraction.\n");
    return result;
  }

  // printf ("convDouble2Long(): fraction AFTER removeSpaces() call is: <%s> +++\n", fraction);

  result = removeSpaces(&whole);
  if(result != ztSuccess){
    fprintf(stderr, "convDouble2Long(): Error failed removeSpaces() function for whole.\n");
    return result;
  }

  // printf ("convDouble2Long(): whole AFTER removeSpaces() call is: <%s> +++\n", whole);

  /* number can not have fraction - all must be ZEROS **/
  if (strspn(fraction, allowed) != strlen(fraction)){
    fprintf(stderr, "convDouble2Long(): Error argument double has fraction.\n");
    return ztInvalidArg;
  }

  /* this should not happen! */
  if (strspn(whole, digits) != strlen(whole)){
    fprintf(stderr, "convDouble2Long(): Error myWhole part of double "
	    "has something other than digits.\n");
    return ztInvalidArg;
  }
  /* now this means *endPtr below is always '\0' - checked below for completion only! */

  /*  from man strtol :
      RETURN VALUE
      The strtol() function returns the result of the conversion, unless the value would underflow or  overflow.
      If  an  underflow  occurs,  strtol()  returns LONG_MIN.   If  an  overflow  occurs,  strtol()  returns  LONG_MAX.
      In both cases, errno is set to ERANGE.  Precisely the same holds for strtoll() (with
      LLONG_MIN and LLONG_MAX instead of LONG_MIN and LONG_MAX).
      *** end from man strtol : NOTE it has been reformatted */

  /* set errno */
  errno = 0;
  numL = (long) strtol (whole, &endPtr, 10);
  if ( *endPtr != '\0' || errno != 0){ /* ERANGE */

    fprintf(stderr, "convDouble2Long() Error failed strtol() call for <%s> !!\n"
	    "with system error message : %s\n", whole, strerror(errno));
    return ztInvalidArg;
  }

  *dstL = numL;

  return ztSuccess;
}

int printBold (char *str){

#define STYLE_BOLD         "\033[1m"
#define STYLE_NO_BOLD   "\033[22m"

  printf(STYLE_BOLD);
  printf("%s", str);
  printf(STYLE_NO_BOLD);

  printf("%s%s%s", STYLE_BOLD, str, STYLE_NO_BOLD);

  return 0;

} /* END printBold (char *str) **/

int isGoodExecutable(char *file){

  /* TODO : use "errno" for complete test */

  /* does file exist and executable?
   * (access(file, F_OK) == 0) --> exist
   * (access(file, R_OK | X_OK) == 0) --> user can execute file
   */
  if ((access(file, F_OK) == 0) &&
      (access(file, R_OK | X_OK) == 0))

    return TRUE;

  return FALSE;

} /* END isGoodExecutable() **/


/* arg2FullPath():
 *
 * function return full path to its argument.
 *
 * argument is usually "file name" entered to program by user.
 *
 */

char *arg2FullPath(const char *arg){

  char *fullPath = NULL;

  /* already path case **/
  if(arg[0] == '/'){

    fullPath = STRDUP(arg);
    return fullPath;
  }

  /* case starts with tilde '~' prepend $HOME **/
  if(arg[0] == '~'){

    char *home;

    home = getHome();
    if(!home){
      fprintf(stderr, "arg2FullPath(): Error failed getHome().\n");
      return fullPath;
    }

    fullPath = (char *)malloc(strlen(home) + strlen(arg) + 1);
    if(!fullPath){
      fprintf(stderr, "arg2FullPath(): Error allocating memory.\n");
      return fullPath;
    }
    memset(fullPath, 0, strlen(home) + strlen(arg) + 1);

    strcpy(fullPath, home);
    strcat(fullPath, arg + 1); /* skip the tilde character **/

    return fullPath;
  }

  /* case starts with double dots '../' **/
  if(strncmp(arg, "../", 3) == 0){

    char *parent = NULL;
    char *current = NULL;

    current = getcwd(NULL, 0);
    if(!current){
      fprintf(stderr, "arg2FullPath(): Error failed getcwd().\n");
      return fullPath;
    }

    parent = getParentDir(current);
    if(!parent){
      fprintf(stderr, "arg2FullPath(): Error failed getParentDir().\n");
      return fullPath;
    }

    fullPath = (char *)malloc(strlen(parent) + strlen(arg) + 1);
    if(!fullPath){
      fprintf(stderr, "arg2FullPath(): Error allocating memory.\n");
      return fullPath;
    }
    memset(fullPath, 0, strlen(parent) + strlen(arg) + 1);

    if(SLASH_ENDING(parent))
      sprintf(fullPath, "%s%s", parent, arg + 3); /* skip '../' **/
    else
      sprintf(fullPath, "%s%s", parent, arg + 2); /* skip '..' - we need slash **/


    free(current);
    free(parent);
    
    return fullPath;
  }

  /* case starts './' AND not slash **/
  if((strncmp(arg, "./", 2) == 0) || (arg[0] != '/')){

    char *current = NULL;

    current = getcwd(NULL, 0);
    if(!current){
      fprintf(stderr, "arg2FullPath(): Error failed getcwd().\n");
      return fullPath;
    }

    fullPath = (char *)malloc(strlen(current) + strlen(arg) + 2);
    if(!fullPath){
      fprintf(stderr, "arg2FullPath(): Error allocating memory.\n");
      return fullPath;
    }
    memset(fullPath, 0, strlen(current) + strlen(arg) + 2);

    if(SLASH_ENDING(current)){

      if(arg[0] == '.')
	sprintf(fullPath, "%s%s", current, arg + 2); /* skip './' **/
      else
	sprintf(fullPath, "%s%s", current, arg);
    }
    else{

      if(arg[0] == '.')
	sprintf(fullPath, "%s%s", current, arg + 1); /* skip '.' and keep slash **/
      else
	sprintf(fullPath, "%s/%s", current, arg); /* insert slash **/
    }
 
    free(current);

    return fullPath;
  }

  return fullPath;

} /* END arg2FullPath() **/
