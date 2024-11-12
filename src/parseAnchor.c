/*
 * parseAnchor.c
 *
 *  Created on: Jun 18, 2023
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "getdiff.h"
#include "util.h"
#include "ztError.h"
#include "fileio.h"
#include "parseAnchor.h"

/* parseHtmlFile():
 * Parses ANCHOR TAGS in html input file pointed to by 'filename' parameter.
 * Extracted strings are inserted into a ALPHABETICALLY SORTED string list
 * specified by parameter 'destList'.
 *
 * Caller initials 'destList'; it is assumed to be empty string list.
 *
 ****************************************************************************/

int parseHtmlFile(STRING_LIST *destList, const char *filename){

  int   result;

  ASSERTARGS(destList && filename);

  if(! TYPE_STRING_LIST(destList)){
    fprintf(stderr, "%s: Error 'destList' parameter is not of type STRING_LT in parseHtmlFile().\n", progName);
    return ztInvalidArg;
  }

  if(DL_SIZE(destList) != 0){
    fprintf(stderr, "%s: Error parseHtmlFile() 'destList' parameter is not empty.\n", progName);
    return ztListNotEmpty;
  }

  result = isFileUsable(filename);
  if(result != ztSuccess){
    fprintf(stderr, "%s: Error parseHtmlFile() failed isUsableFile() function.\n"
	    "filename: <%s>\n", progName, filename);
    return result;
  }

  /* read input file into fileList - string list **/
  STRING_LIST   *fileList;

  fileList = initialStringList();

  result = file2StringList(fileList, filename);
  if(result != ztSuccess)
    return result;

  if(isHtmlStringList(fileList) == FALSE){
    fprintf(stderr, "%s: Error parameter 'filename' <%s> is not HTML file", progName, filename);
    return ztInvalidArg;
  }

  /* forward lines with ANCHOR TAGS to parseAnchor() function **/
  ELEM   *elem;
  char   *line;

  elem = DL_HEAD(fileList);
  while(elem){

    line = (char *)DL_DATA(elem);

    if(strstr(line, ANCHOR_TAG)){

      result = parseAnchor(destList, line);
      if(result != ztSuccess){
	fprintf(stderr, "%s: Error failed parseAnchor() function.\n", progName);
	return result;
      }
    }

    elem = DL_NEXT(elem);
  }

  return ztSuccess;

} /* END parseHtmlFile() **/

/* parseAnchor_strtok() : parses ANCHOR TAG using strtok() library function.
 * THIS FUNCTION IS NOT USED.
 *
 * Function parseAnchor() does the same without using strtok().
 *
 ******************************************************************************/

int parseAnchor_strtok(STRING_LIST *destList, char *line){

  char   *myLine;
  char   *anchorStart;
  char   *delimiter = "><";
  char   *token;

  ASSERTARGS(destList && line);

  myLine = STRDUP(line);

  anchorStart = strstr(myLine, ANCHOR_TAG);
  if(!anchorStart){
    fprintf(stderr, "%s: Error could not find anchor start.\n", progName);
    return ztInvalidArg;
  }

  while(anchorStart){

    anchorStart = strchr(anchorStart, '>');

    myLine = strstr(myLine, ANCHOR_END); /* move pointer BEFORE strtok() call **/
    if(!myLine)
      return ztUnknownError;

    myLine++; /* start character is going to be over written with null by strtok() **/

    token = strtok(anchorStart, delimiter);
    if(!token){

      return ztUnknownError;
    }

    if(isdigit(token[0])){
      ListInsertInOrder(destList, token);
    }

    anchorStart = strstr(myLine, ANCHOR_TAG);
  }

  return ztSuccess;

} /* END parseAnchor_strtok() **/

int parseAnchor(STRING_LIST *destList, char *line){

  char   *myLine;
  char   *anchorStart;

  char   gt_char = '>';
  char   lt_char = '<';

  char   *anchorEnd;
  char   *anchorClosing;
  int    length;
  char   buffer[256];
  char   *string;

  ASSERTARGS(destList && line);


  myLine = STRDUP(line);

  anchorStart = strstr(myLine, ANCHOR_TAG);
  if(!anchorStart){
    fprintf(stderr, "%s: Error could not find ANCHOR TAG in 'line'.\n", progName);
    return ztInvalidArg;
  }

  while(anchorStart){

    anchorEnd = strchr(anchorStart, gt_char);

    anchorClosing = strchr(anchorEnd, lt_char);

    if ( ! (anchorEnd && anchorClosing)){
      fprintf(stderr, "%s: Error could not get anchor tag opening OR closing.\n", progName);
      return ztUnknownError;
    }

    length = anchorClosing - anchorEnd;

    if(length > 256){
      fprintf(stderr, "%s: Error buffer fixed length size is small for string.\n", progName);
      return ztFatalError; /* need small buffer error code! **/
    }

    memset(buffer, 0, sizeof(char) * sizeof(buffer));

    sprintf (buffer, "%.*s",  length - 1, anchorEnd + 1);
    /* -1 & +1 are to drop lt & gt characters **/

    if(isdigit(buffer[0])){

      string = STRDUP(buffer);

      ListInsertInOrder(destList, string);
    }

    anchorStart = strstr(anchorClosing, ANCHOR_TAG);

  }

  return ztSuccess;

} /* END parseAnchor0() **/


/* isHtmlStringList():
 *  */

int isHtmlStringList(STRING_LIST *list){

  ELEM *elem;
  char *firstLine, *secondLine, *lastLine;

  ASSERTARGS (list);

  if(! TYPE_STRING_LIST(list))

    return FALSE;

  if(DL_SIZE(list) < 3)

    return FALSE;

  /* <html> tag must appear at first or second line **/

  elem = DL_HEAD(list);

  firstLine = (char *)DL_DATA(elem);

  secondLine = (char *)DL_DATA(DL_NEXT(elem));

  if(! (strstr(firstLine, HTML_TAG) || strstr(secondLine, HTML_TAG)))

    return FALSE;

  /* </html> tag must appear on last line - STRING_LIST has no empty lines **/

  elem = DL_TAIL(list);

  lastLine = (char *)DL_DATA(elem);

  if( ! strstr(lastLine, HTML_END))

    return FALSE;

  return TRUE;

} /* END isHtmlStringList() **/
