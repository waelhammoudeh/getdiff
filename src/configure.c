/*
 * configure.c
 *
 *  Created on: Apr 25, 2023
 *      Author: wael
 *
 * Simple configuration file implementation:
 * Configuration file is a text file with a pair of KEY and VALUE strings on each
 * line separated by space, tab and or an optional equal '=' sign.
 *
 * Specifications:
 * Maximum length of a line is 128 characters and should be terminated
 * by a line feed character including last line in the file.
 * The file may have three kinds of lines:
 * 1) Blank or empty line: all white spaces to ease readability.
 * 2) Configure line: has the key and value as explained below.
 * 3) Comment line: first non-space character is either ';' semicolon or '#'
 *    hash mark character. No comment is allowed on configure line.
 *
 *  Configure line: starts with string for key name followed by space and an
 *  optional equal sign followed by space then string value for that key.
 *
 *  Key string specifications:
 *  The key string has a maximum string length of 16 characters made of all
 *  capital letters, digits, and the dash and underscore [- _] characters. No
 *  other characters are allowed in key string.
 *  Key string can NOT start or end with underscore or dash.
 *
 *  User name and password are limited to maximum string length of 64 characters.
 *
 *  This implementation checks the validity of the values it finds; for example
 *  is a filename valid filename - does NOT check if filename exists in the system.
 *
 *  Duplicate keys are NOT allowed in configuration file.
 *
 *  Usage:
 *
 *  1) User or client must setup a CONF_ENTRY array with last entry in the array
 *  has all members set to NULL or zero. In each entry user must provide the key
 *  string and configure type for that entry.
 *
 *  2) User then calls initialConf() function with first argument is the pointer
 *  to filled CONF_ENTRY array and second argument is the number of actual entries
 *  in the array; do not include terminating entry in this number. This function
 *  checks the key string for each entry and sets the index member to that entry
 *  in the array.
 *
 *  3) User then calls configureGetValues() function with first parameter as the
 *  pointer to initialed CONF_ENTRY array above, second parameter is a pointer
 *  to an integer of found entries and last parameter is a pointer to character
 *  string with configuration file name and path.
 *
 *  4) Key strings may and may not be in configuration file.
 *
 *  5) It is an ERROR to find a key string in configuration file that is NOT in
 *  the initialed CONF_ENTRY array.
 *
 *  6) When configureGetValues() returns with ztSuccess, the string for the
 *  value member of each found entry should be set in the array. Second parameter
 *  'numFound' is set to the number of entries found in the configuration file.
 *
 *  7) When the 'value' is found to be invalid for any key, the exported variable
 *  'confErrLineNum' is set to the line number in the configuration file where that
 *  invalid value was found. User should set 'confErrLineNum' to zero before
 *  the call to 'configureGetValues()' function.
 *
 *  NOTE: client does not call myfgets() function, so we do not mention the exported
 *  variable 'myFgetsError' here!
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "configure.h"
#include "util.h"
#include "ztError.h"

/* exported global variable **/
int myFgetsError;
int confErrLineNum;

/* To use configure; client sets up array of CONF_ENTRY, one entry structure
 * for each configuration key, client provide the KEY string and its CONF_TYPE,
 * the later is used for basic checking of values for that entry; for FILE_CT
 * the value is checked for good file name only, DIR_CT is checked for good
 * directory name only and so on.
 * The array should end with a terminating entry like {NULL, NULL, 0, 0}.
 *
 *****************************************************************************/

/* initialConf(): function verifies CONF_ENTRY array setup by caller / client.
 *
 * Caller provide the key string and its CONF_TYPE.
 *
 * Parameters: entries is a pointer to CONF_ENTRY array filled by caller with
 * key and cType members set. The key member is the string for entry key and
 * cType is the the CONF_TYPE for that entry.
 * The numEntries parameter is the actual number of entries in the array. Note this
 * does NOT include terminating entry.
 *
 * CONF_ENTRY array should end with a terminating entry like {NULL, NULL, 0, 0}.
 *
 ************************************************************************/
int initialConf(CONF_ENTRY *entries, int numEntries){

  ASSERTARGS (entries);

  if(numEntries < 1){
    fprintf(stderr,"initialConf(): Error invalid numEntires argument.\n");
    return ztInvalidArg;
  }

  CONF_ENTRY    *mover;

  /* check for terminating entry; must be at array index == numEntries **/
  mover = &(entries[numEntries]);

  if(mover->key != NULL || mover->value != NULL ||
     mover->ctype != 0 || mover->index != 0)

    return ztConfInvalidArray;

  mover = entries;
  while(mover->key){

    if( ! isOkayKey(mover->key))

      return ztConfBadKeyString;

    /* value is NULL initially **/
    if(mover->value != NULL)

      return ztConfInvalidValue;

    if ( ! OK_CONF_TYPE(mover->ctype) )

      return ztConfInvalidType;

    mover++;
  }

  /* all okay; now set index - client may use index to find entry? **/
  mover = entries;
  for(int i=0; i < numEntries; i++, mover++)

    mover->index = i;

  return ztSuccess;

} /* END initialConf() **/

/* isOkayKey(): checks key string for allowed characters and string length.
 * Key string length should be no more than 16 characters - MAX_KEY_LENGTH.
 * The string is ALL capital letters, numbers, may include dash or underscore characters.
 * Returns: TRUE or FALSE.
 *
 ******************************************************************/
int isOkayKey(char const *key){

  char *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-_0123456789";
  char dash = '-';
  char underscore = '_';

  ASSERTARGS(key);

  if(strlen(key) == 0 || strlen(key) > MAX_KEY_LENGTH)

    return FALSE;

  if(strspn(key, allowed) != strlen(key)) /* has disallowed character **/

    return FALSE;

  /* KEY can not start or end with dash or underscore **/
  if( (key[0] == dash) ||
      (key[0] == underscore) ||
      (key[strlen(key) -1] == dash) ||
      (key[strlen(key) -1] == underscore) )

    return FALSE;

  /* KEY cannot start with a digit **/
  if(isdigit(key[0]))

    return FALSE;

  return TRUE;

} /* END isOkayKey() **/

void fprintCEntry(FILE *toFile, CONF_ENTRY *entry){

  FILE    *fPtr;

  ASSERTARGS(entry);

  if(toFile)

    fPtr = toFile;

  else

    fPtr = stdout;

  if(entry->key)
    fprintf(fPtr, "Key is: %s\n", entry->key);
  else
    fprintf(fPtr, "Key is not set.\n");

  if(entry->value)
    fprintf(fPtr, "Value is: %s\n", entry->value);
  else
    fprintf(fPtr, "Value is not set.\n");

  if ( ! OK_CONF_TYPE(entry->ctype))

    fprintf(fPtr, "Configure Type is invalid.\n");

  else {

    fprintf(fPtr, "Configure Type is:");

    switch (entry->ctype) {
    case FILE_CT:

      fprintf(fPtr, "FILE_CT.\n");
      break;

    case DIR_CT:

      fprintf(fPtr, "DIR_CT.\n");
      break;

    case INET_URL_CT:

      fprintf(fPtr, "INET_URL_CT.\n");
      break;

    case PORT_CT:

      fprintf(fPtr, "PORT_CT.\n");
      break;

    case NAME_CT:

      fprintf(fPtr, "NAME_CT.\n");
      break;

    case BOOL_CT:

      fprintf(fPtr, "BOOL_CT.\n");
      break;

    case NONE_CT:

      fprintf(fPtr, "NONE_CT.\n");
      break;

    case DIGITS9_CT:

      fprintf(fPtr, "DIGITS9_CT.\n");
      break;

    case ANY_CT:

      fprintf(fPtr, "ANY_CT.\n");
      break;

    default:

      break;
    }
  }

  fprintf(fPtr, "Index is: %d.\n\n", entry->index);

  return;

} /* END fprintEntry() **/

/* fprintCArray(): prints the array of CONF_ENTRY with number of entries in
 * numEntries to the open file with *toF FILE pointer.
 *
 * No error checking is done, assuming initialConf() was called before!
 *
 ***************************************************************/

void fprintCArray(FILE *toF, CONF_ENTRY *array, int numEntries){

  CONF_ENTRY    *mover;

  ASSERTARGS(array);

  if(numEntries < 1)

    return;

  mover = array;

  for(int i = 0; i < numEntries; i++, mover++)

    fprintCEntry(toF, mover);


  return;

} /* END fprintCArray() **/

/* boolStr2Integer(): boolean string to integer; function converts string to
 * integer.
 *
 * Returns: ztSuccess or ztInvalidArg on error.
 *
 **********************************************************************/

int boolStr2Integer(int *dest, char const *str){

  ASSERTARGS (dest && str);

  if (strlen(str) > 5) //largest string "false"

    return ztInvalidArg;

  if ( (strcasecmp(str, "true") == 0) ||
       (strcasecmp(str, "on") == 0) ||
       (strcmp(str, "1") == 0))

    *dest = 1;

  else if ((strcasecmp(str, "false") == 0) ||
	   (strcasecmp(str, "off") == 0) ||
	   (strcmp(str, "0") == 0))

    *dest = 0;

  else

    return ztInvalidArg;

  return ztSuccess;

} /* END boolStr2Integer() **/

/* isOkayBoolValue(): returns ztSuccess if value is one in set below:
 *  valid values set: [true, on, 1, false, off, 0]
 * otherwise it returns ztInvalidArg.
 *
 **********************************************************************/

int isOkayBoolValue(const char *value){

  ASSERTARGS(value);

  if(strlen(value) > 5) //largest string "false"

    return ztInvalidArg;

  if( (strcasecmp(value, "true") == 0) ||
      (strcasecmp(value, "on") == 0) ||
      (strcmp(value, "1") == 0) ||
      (strcasecmp(value, "false") == 0) ||
      (strcasecmp(value, "off") == 0) ||
      (strcmp(value, "0") == 0) ){

    return ztSuccess;
  }

  return ztInvalidArg;

} /* END isOkayBoolValue() **/

/* myfgets(): works some how like fgets() with the following exceptions:
 *  1) Reads in at most CLINE_MAX (128) characters.
 *  2) Drops read in linefeed character.
 *  3) Removes leading and trailing white spaces.
 *  4) Skips lines with ALL white spaces string (empty lines).
 *  5) Skips lines where first non-space [tab or space] character is either
 *     semicolon ';' or hash mark '#'.
 *  6) Sets lineNum pointer parameter to the original line number in the file.
 *  7) Function returns NULL on error and sets exported variable myFgetsError
 *     as follows:
 *
 *      - ztMemoryAllocate on memory allocation error.
 *      - ztEndOfFile: when end-of-file indicator is set
 *      - ztFileError: when file error indicator is set
 *      - ztUnknownError: any other error
 *      - ztNoLinefeed: line is longer than CLINE_MAX, this is an error.
 *
 *
 * function allocates memory for *destStr and this is the return value too.
 * function does NOT read or return partial line.
 *
 ************************************************************************/

char *myfgets(char **destStr, int *lineNumber, FILE *filePtr){

  char   *myStr = NULL;
  char   *fgetsResult;
  char   buffer[CLINE_MAX + 1] = {0};

  static int   localLineNumber = 0;


  char    *whiteSpaceSet = "\040\t\n\r";

  /* white space set: space, tab, linefeed and carriage return.
   * comment set: ';' and '#' characters */

  char    *commentSet = ";#";

  ASSERTARGS(destStr && lineNumber && filePtr);

  *destStr = NULL;

  while(1){

    fgetsResult = fgets(buffer, CLINE_MAX + 1, filePtr);

    if(fgetsResult == NULL){
      if(feof(filePtr))
        myFgetsError = ztEndOfFile;
      else if(ferror(filePtr))
        myFgetsError = ztFileError;
      else
        myFgetsError = ztUnknownError;

      return NULL;
    }

    localLineNumber++;

    myStr = buffer;

    /* ignore all white space line - empty line **/
    if (strspn(myStr, whiteSpaceSet) == strlen(myStr))

      continue;

    /* remove leading spaces **/
    while (isspace((unsigned char) myStr[0]))

      myStr++;

    /* ignore comment lines **/
    if ((strpbrk(myStr, commentSet) == myStr))

      continue;

    break;
  }

  /* fgets() reads up to linefeed character or PATH_MAX -1 characters,
   * line was too long if there is no linefeed at the end - in linux system. */
  if(myStr[strlen(myStr) - 1] != '\n'){
	fprintf(stderr, "myfgets(): Error partial line read, small buffer here, "
			"line longer than [128] characters.\n");

	myFgetsError = ztNoLinefeed; // need small buffer error code!
	return NULL;
  }

  /* drop linefeed character **/
  myStr[strlen(myStr) - 1] = '\0';

  while (isspace((unsigned char) myStr[strlen(myStr) - 1]))

	myStr[strlen(myStr) - 1] = '\0';

  /* allocate memory for destination string **/
  *destStr = (char *)malloc(sizeof(char) * (strlen(myStr) + 1));
  if( ! *destStr){
	myFgetsError = ztMemoryAllocate;
	return NULL;
  }

  strcpy(*destStr, myStr);

  /* set client lineNum **/
  *lineNumber = localLineNumber;

  return *destStr;

} /* END myfgets() **/

int file2LineInfoList(DLIST *list, char const *filename){

  FILE      *fPtr;
  LINE_INFO *lineInfo;
  int       myLineNum;

  char   *myLine;
  char   *chResult;

  int    result;

  ASSERTARGS(list && filename);

  if(DL_SIZE(list) != 0){
    fprintf(stderr, "file2LineInfoList(): Error parameter 'list' is not for an empty list.\n");
    return ztListNotEmpty;
  }

  /* file must exist, readable AND we own it. **/
  result = isFileUsable(filename);
  if(result != ztSuccess){
    fprintf(stderr, "file2LineInfoList(): Error file in parameter 'filename' is not usable.\n"
    		" - We must own the file! File is not usable for: <%s>\n", ztCode2Msg(result));
    return result;
  }

  errno = 0;

  fPtr = fopen(filename, "r");
  if (fPtr == NULL){
    fprintf (stderr, "file2LineInfoList(): Error opening file: <%s>.\n", filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  /* set exported variable myFgetsError to zero **/
  myFgetsError = ztSuccess;

  chResult = myfgets(&myLine, &myLineNum, fPtr);
  while(chResult){

	/* handle error first **/
    if(myFgetsError != ztSuccess){
      if(myFgetsError == ztEndOfFile){
//        fprintf(stdout, "file2LineInfoList(): myfgets() function reached end of file.\n");
        return ztSuccess;
      }
      else if(myFgetsError == ztFileError){
        fprintf(stderr, "file2LineInfoList(): Error failed myfgets() function with "
                        "file error marker set!\n");
        return ztFileError;
      }
      else if(myFgetsError == ztMemoryAllocate){
        fprintf(stderr, "file2LineInfoList(): Error failed myfgets() function with "
                        "memory allocation error!\n");
        return ztMemoryAllocate;
      }
      else if(myFgetsError == ztNoLinefeed){
    	fprintf(stderr, "file2LineInfoList(): Error failed myfgets() function with"
                        " no linefeed error.\n");
    	return ztNoLinefeed;
      }
      else if((myFgetsError == ztUnknownError) || (!chResult)){
        fprintf(stderr, "file2LineInfoList(): Error failed myfgets() function with"
                        " Unknown Error!\n");
        return ztUnknownError;
      }
      //else if(!chResult) break;

    } /* end error handling **/

    lineInfo = (LINE_INFO *)malloc(sizeof(LINE_INFO));
    if(!lineInfo){
      fprintf(stderr, "file2LineInfoList(): Error allocating memory.\n");
      return ztMemoryAllocate;
    }

    lineInfo->string = STRDUP(myLine);
    lineInfo->originalNum = myLineNum;

    result = insertNextDL(list, DL_TAIL(list), (void *) lineInfo);
    if(result != ztSuccess){
      fprintf(stderr, "file2LineInfoList(): Error failed insertNextDL() function.\n");
      return result;
    }

    /* fprintf(stdout, "file2LI_List(): myLineNum is: <%d> myLine is: <%s>\n\n", myLineNum, myLine); **/

    /* get next line from file **/
    chResult = myfgets(&myLine, &myLineNum, fPtr);

  } /* end while(chResult) **/

  return ztSuccess;

} /* END file2LineInfoList() **/

/* zapLineInfo() : destroys and frees LINE_INFO structure
 *
 *
 *************************************************************/

void zapLineInfo(void **data){

  LINE_INFO *lineInfo;

  ASSERTARGS(data);

  lineInfo = (LINE_INFO *) *data;

  if(lineInfo && lineInfo->string){

    free(lineInfo->string);
    memset(lineInfo, 0, sizeof(LINE_INFO));
  }

  if(lineInfo)

    free(lineInfo);

  return;

} /* END zapLineInfo() **/

void fprintLineInfo(FILE *toPtr, LINE_INFO *lineInfo){

  FILE  *filePtr = stdout; /* write to terminal **/

  ASSERTARGS(lineInfo);

  if( toPtr ) /* if caller set, use it **/

    filePtr = toPtr;

  fprintf(filePtr, "%d: <%s>\n", lineInfo->originalNum, lineInfo->string);

  return;

} /* END fprintLineInfo() **/

void fprintLineInfoList(FILE *dstFP, DLIST *list){

  FILE    *filePtr = stdout;

  LINE_INFO    *lineInfo;
  ELEM    *elem;

  ASSERTARGS(list);

  if(dstFP)

    filePtr = dstFP;

  if(DL_SIZE(list) == 0){

    fprintf(filePtr, "fprintLineInfoList(): Parameter list is empty.\n");
    return;
  }

  fprintf(filePtr, "fprintLineInfoList(): Printing LINE_INFO list with size = <%d>\n", DL_SIZE(list));

  elem = DL_HEAD(list);
  while(elem){

    lineInfo = (LINE_INFO *) DL_DATA(elem);

    fprintLineInfo(filePtr, lineInfo);

    elem = DL_NEXT(elem);

  }

  return;

} /* END fprintLineInfoList() **/

/* configureGetValues():
 * function sets CONF_ENTRY member 'value' in the array ceArray from file 'confFile',
 * numFound is set to number of entries found in 'confFile'.
 *
 * Parameters:
 *  - ceArray: a pointer to CONF_ENTRY array; with last entry zeroed out.
 *  - numFound: a pointer to integer where number of found entries is returned.
 *  - confFile: string character pointer for configuration file (path + name).
 *
 * It is an error if key is specified more than once in configuration file.
 *
 * THIS FUNCTION IS TOO LONG?
 *
 ******************************************************************************/
int configureGetValues(CONF_ENTRY *ceArray, int *numFound, char *confFile){

  int    result;

  ASSERTARGS(ceArray && numFound && confFile);

  /* read configure file into LINE_INFO list **/
  DLIST    *confList;

  confList = (DLIST *) malloc(sizeof(DLIST));
  if ( ! confList ){
    fprintf(stderr, "configureGetValues(): Error allocating memory.\n");
    return ztMemoryAllocate;
  }
  initialDL (confList, zapLineInfo, NULL);

  /* read file into list - data pointer in element is a pointer to LINE_INFO **/
  result = file2LineInfoList(confList, confFile);
  if(result != ztSuccess){
    fprintf(stderr, "configureGetValues(): Error failed file2LineInfoList() function.\n");
    return result;
  }

  /* configure file maybe empty or all comments - nothing to do. **/
  if(DL_SIZE(confList) == 0){
    fprintf(stdout, "configureGetValues(): Configuration file is empty or all comments.\n");
    destroyDL(confList);
    free(confList);
    return ztSuccess;
  }

  /* get key and value tokens **/
  char    *delimiterKey = "=\040\t"; /* set [= SPACE and TAB] **/
  char    *keyTok;
  char    *delimiterValue = "=\040\t\r\n";
  char    *valueTok;
  ELEM    *elem;
  LINE_INFO    *lineInfo;
  char    *myString;
  CONF_ENTRY    *entry;

  STRING_LIST   *foundKeysList; /* do not allow duplicate keys -
                                   string character array not list? **/

  foundKeysList = initialStringList();
  if( ! foundKeysList){
	fprintf(stderr, "configureGetValues(): Error failed initialStringList() function.\n");
	return ztMemoryAllocate;
  }

  /* ensure numFound is zero **/
  *numFound = 0;

  elem = DL_HEAD(confList);
  while(elem){

    lineInfo = (LINE_INFO *)DL_DATA(elem);
    if( ! (lineInfo && lineInfo->string)){
      fprintf(stderr, "configureGetValues(): Error null pointer for lineInfo OR lineInfo->string.\n");
      return ztUnknownError;
    }

    myString = STRDUP(lineInfo->string);

    keyTok = strtok(myString, delimiterKey);
    valueTok = strtok(NULL, delimiterValue);

    /* a key must have a value **/
    if( ! (keyTok && valueTok) ){
      fprintf(stderr, "configureGetValues(): Error failed to get key token or value token.\n");
      return ztParseError;
    }
/*
printf("configureGetValues():\n"
		"   lineInfo->string :   <%s>\n"
		"   keyTok is:   <%s>\n"
		"   valueTok is: <%s>\n\n", lineInfo->string, keyTok, valueTok);
**/
    /* did we see this key before in this file? **/
    if(isStringInList(foundKeysList, keyTok) == TRUE){
      fprintf(stderr, "configureGetValues(): Error found duplicate entries for key <%s>"
    		  " in configuration file\n", keyTok);
      return ztConfDupKey;
    }

    result = insertNextDL(foundKeysList, DL_TAIL(foundKeysList), (void *) keyTok);
    if(result != ztSuccess){
      fprintf(stderr, "configureGetValues(): Error failed insertNextDL() function.\n");
      return result;
    }

    /* it is an error if KEY is not in the initialed array **/
    entry = findEntryByKey(keyTok, ceArray);
    if ( ! entry ){
      fprintf(stderr, "configureGetValues(): Error unrecognized key in configure file: <%s>\n", keyTok);
      return ztConfUnregonizedKey;
    }

    result = setConfValue(entry, valueTok);
    if(result != ztSuccess){
      /* set line number with error **/
      confErrLineNum = lineInfo->originalNum;
      fprintf(stderr, "Error in configuration file for key: <%s>\n "
    		  " Line number in configuration file: <%d>\n", entry->key, confErrLineNum);
      return result;
    }

    (*numFound)++; /* increment found number **/
    elem = DL_NEXT(elem);

  } /* end while(elem) **/

  /* cleanup **/
  destroyDL(confList);
  free(confList);

  destroyDL(foundKeysList);
  free(foundKeysList);

  return ztSuccess;

} /* END configureGetValues() **/

/* findEntryByKey(): returns a pointer to CONF_ENTRY in array confArray
 * that matches string 'key'
 **********************************************************************/

CONF_ENTRY *findEntryByKey(char const *key, CONF_ENTRY *confArray){

  CONF_ENTRY    *arrayMover;

  ASSERTARGS(key && confArray);

  arrayMover = confArray;

  while(arrayMover->key){

    if (strcmp(key, arrayMover->key) == 0)

      return arrayMover;

    arrayMover++;

  }

  return NULL;

} /* END isKeyInConfArray() **/

/* setConfValue():
 *  - checks value based on entry CONF_TYPE
 *  - value is set with STRDUP() when it checks okay
 *  - STRDUP() terminates program on memory failure.
 *
 *******************************************************/

int setConfValue(CONF_ENTRY *entry, char const *value){

  ASSERTARGS(entry && value);

  if(strlen(value) == 0)

    return ztInvalidArg;

  CONF_TYPE    confType = entry->ctype;

  /* Note: value is set after testing in switch statement **/

  switch (confType){

  case FILE_CT:

    if(isGoodFilename(value) != ztSuccess)

      return ztConfInvalidValue;

    break;

  case DIR_CT:

    if(isGoodDirName(value) != ztSuccess)

      return ztConfInvalidValue;

    break;

  case INET_URL_CT:

    if(isOkayFormat4HTTPS(value) != TRUE)

      return ztConfInvalidValue;

    break;

  case PORT_CT:

    if(isGoodPortString(value) != ztSuccess)

      return ztConfInvalidValue;

    break;

  case NAME_CT:

    if(strlen(value) > MAX_USER_NAME)

      return ztConfInvalidValue;

    break;

  case BOOL_CT:

    if(isOkayBoolValue(value) == ztInvalidArg)

      return ztConfInvalidValue;

    int num;

    boolStr2Integer(&num, value);

    if(num == 1) // THIS IS WRONG
    	entry->value = STRDUP("1");
    else
    	entry->value = STRDUP("0");

    break;

  case NONE_CT:

    if( (strcasecmp(value, "none") != 0) &&
	(strcasecmp(value, "off") != 0) )

      return ztConfInvalidValue;

    break;

  case DIGITS9_CT:

    char   *allowed = "0123456789";

    if(strlen(value) > 9 || (strlen(value) < 4) ||
      (strspn(value, allowed) != strlen(value)) ||
	  value[0] == '0'){

      fprintf (stderr, "Error in configuration file; invalid sequence number for \"BEGIN\" value.\n"
                       "Valid sequence number is all digits with length between [4 - 9] "
                       "digits and does not start with '0'.\n"
                       "Invalid value found: [%s].\n", value);
      return ztConfInvalidValue;
    }
    break;

  case ANY_CT:

    break;

  default:

    return ztInvalidArg;
  }

  /* value passed test, set it now **/
  entry->value = STRDUP(value);

  return ztSuccess;

} /* END setConfValue() **/
