/*
 * parsePage.c
 *
 *  Created on: Feb 19, 2022
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getdiff.h"
#include "util.h"
#include "ztError.h"
#include "list.h"
#include "fileio.h"
#include "parseHtml.h"

static ELEM *find_strDL (DLIST *src, char *string);

/* dstList is a string list */
int parseIndexPage (DLIST *dstList, char *filename){

	int	        result;
	DLIST	*srcListLI; /* ending with LI : data pointer in element is for LINE_INFO  */
	ELEM	*elem, *startElem;
	LINE_INFO	*lineInfo;

	char		*ul, *table; /* flags what to parse */
	char		*line;

	ASSERTARGS (dstList && filename);

	if (IsArgUsableFile(filename) != ztSuccess){

		fprintf(stderr, "%s: Error failed call to IsArgUsableFile() <%s>\n",
				progName, filename);
		return ztFileNotFound;
	}

	// destination list should be empty
	if (dstList->size)

		return ztListNotEmpty;

	srcListLI = (DLIST *) malloc(sizeof(DLIST));
	if (!srcListLI){

		fprintf (stderr, "%s: Error allocating memory.\n", progName);
        return ztMemoryAllocate;
	}
	initialDL(srcListLI, zapLineInfo, NULL);

	/* file2List() discards empty and blank lines */
	result = file2List(srcListLI, filename);
	if (result != ztSuccess){

		fprintf(stderr, "%s: Error reading file into list.\n", progName);
		return result;
	}

	/* page has to have more than ten lines.
	 * we ignore tags start and end spanning multiple lines
	 *****/
	if (DL_SIZE(srcListLI) < 10){
		fprintf(stderr, "%s: Error source file list has very few lines < 10 .\n", progName);
		return ztMissFormatFile;
	}

	if (! isHtmlFileList(srcListLI) ){
		fprintf(stderr, "%s: Error source file is not an html page.\n", progName);
		return ztMissFormatFile;
	}

	/* geofabrik.de has two servers:
	 * 		https://download.geofabrik.de
	 * 		https://osm-internal.download.geofabrik.de
	 * 	index page in first server uses table structure, and second server uses
	 * 	unsorted list structure. Find out what structure we have now.
	 *********************************************************************************/

	ul = table = NULL;

	elem = DL_HEAD(srcListLI);
	while (elem){

		lineInfo = (LINE_INFO *) DL_DATA(elem);
		line = (char *) lineInfo->string;

		if ( ! ul ) /* set flag ONCE only */
			ul = strstr(line, UNSORTED_LIST_TAG);

		if ( ! table )
			table = strstr(line, TABLE_TAG);

		if (ul && table){
			fprintf(stderr, "%s: Error source file has both <ul> and <table> tags. "
					"One is assumed. File format may have changed!\n", progName);
			return ztMissFormatFile;
		}


		elem = DL_NEXT(elem);
	}

	if ( ! (ul || table) ) {
		fprintf(stderr, "%s: Error source file has neither <ul> or <table> tag. "
				"One tag is assumed! File format may have changed!\n", progName);
		return ztMissFormatFile;
	}

	if (ul){

		startElem = find_strDL (srcListLI, LIST_ITEM_TAG);
		ASSERTARGS (startElem);

		result = parseByTag (dstList, startElem, parseLI);
		if ( result != ztSuccess){
			fprintf(stderr, "%s: Error returned from parseByTag() + parseUL. "
					"File format may have changed!\n", progName);
			return result;
		}
	}

	else if (table){

		startElem = find_strDL (srcListLI, TABLE_ROW);
		ASSERTARGS (startElem);

		result = parseByTag (dstList, startElem, parseTablerow);
		if ( result != ztSuccess){
			fprintf(stderr, "%s: Error returned from parseByTag() + parseTable. "
					"File format may have changed!\n", progName);
			return result;
		}
	}

	return ztSuccess;

} /* END parseIndexPage() */

int parseListItem(DLIST *dst, char	*line){

	char		*startTag, *endTag;
	char		*attTag, *attEnd;
	char		*myLine;
	char		*ptr1, *ptr2;
	int		length;
	int		result;
	char		*filename;

	char		buf[MAX_NAME_LENGTH] = {0};

	ASSERTARGS (dst && line);

	//printf("%s\n", line);

	startTag = strstr(line, LIST_ITEM_TAG);
	if( ! startTag )

		return ztInvalidArg;

	endTag = strstr(line, LIST_ITEM_END);
	if( ! endTag )

		return ztInvalidArg;

	attTag = strstr(line, ATTRIBUTE_TAG);
	if( ! attTag )

		return ztInvalidArg;

	attEnd = strstr(line, ATTRIBUTE_TAG);
	if( ! attEnd)

		return ztInvalidArg;

	myLine = strdup(line);

	// move just after <li> tag
	myLine = myLine + strlen(LIST_ITEM_TAG);

	ptr1 = strchr(myLine, '>');
	ptr1++;
	ptr2 = strstr(myLine, ATTRIBUTE_END);
	if ( ! (ptr1 && ptr2) )

		return ztInvalidArg;

	length = ptr2 - ptr1;

	sprintf (buf, "%.*s",  length, ptr1);
	buf[length] = '\0';

	filename = (char *) malloc(length * sizeof(char));
	strcpy(filename, buf);

	//printf("%s\n", buf);

	if (filename[0] != '.'){

		result = ListInsertInOrder (dst, filename);
		if (result != ztSuccess){
			fprintf(stderr, "%s: Error returned from ListInsertInOrder().\n", progName);
			return result;
		}
	}

	//printf("%s\n", buf);

	return ztSuccess;
}

/* isHtmlFileList(): is the file list pointed to by list for HTML file?
 * first tag has to be <html> and it has to have closing </html> end tag */

int isHtmlFileList (DLIST *list){

	ELEM		*elem;
	LINE_INFO	*lineInfo;
	char		*line;
	char		*lineLower;
	int		result;
	int		countStart, countEnd;


	ASSERTARGS (list);

	if (DL_SIZE(list) < 2)

		return FALSE;

	elem = DL_HEAD(list);
	lineInfo = (LINE_INFO *) DL_DATA(elem);
	line = (char *) lineInfo->string;

	result = stringToLower (&lineLower, line);
	if (result != ztSuccess){

		fprintf(stderr, "%s: Error returned by stringToLower().\n", progName);
		return FALSE;
	}

	removeSpaces (&lineLower);

	/* if we have info tag, line must include "html" substring */
	if ( (strncmp(lineLower, INFO_TAG_START, strlen(INFO_TAG_START)) == 0) &&
			! strstr (lineLower, "html") )

		return FALSE;

	/* file must have one html start tag and one html end tag */
	countStart = 0;
	countEnd = 0;
	elem = DL_HEAD(list);
	while (elem){

		lineInfo = (LINE_INFO *) DL_DATA(elem);
		line = (char *) lineInfo->string;
		stringToLower (&lineLower, line);
		removeSpaces (&lineLower);

		if (strcmp(lineLower, HTML_TAG) == 0)

			countStart++;

		if (strcmp(lineLower, HTML_END) == 0)

			countEnd++;

		if (countStart > 1 || countEnd > 1)

			return FALSE;

		elem = DL_NEXT(elem);
	}

	return TRUE;
}

int parseByTag (DLIST *dstList, ELEM *start,
		                     int (*parseFunc) (DLIST *dstList, char *string)){

	ELEM		*elem;
	LINE_INFO	*lineInfo;
	char	    		*line;
	int				result;
	char				*endTag; /* where we stop the loop */

	ASSERTARGS (dstList && start && parseFunc);

	lineInfo = (LINE_INFO *) DL_DATA(start);
	line = (char *) lineInfo->string;

	if (strncmp(line, LIST_ITEM_TAG, strlen(LIST_ITEM_TAG)) == 0)

		endTag = UNSORTED_LIST_END;

	else if (strncmp(line, TABLE_ROW, strlen(TABLE_ROW)) == 0)

		endTag = TABLE_END;

	elem = start;
	while (elem){

		lineInfo = (LINE_INFO *) DL_DATA(elem);
		line = (char *) lineInfo->string;

		if (strcmp(line, endTag) == 0) /* assumes </ul> && </table> on own line! */

			break;

		result = parseFunc (dstList, line);
		if (result != ztSuccess){
			fprintf (stderr, "%s: Error returned from parseFunc() with line: [ %s ]\n",
					progName, line);
			return result;
		}

		elem = DL_NEXT (elem);

	}

	return ztSuccess;
}

/* dstList has the element data pointer to character string.
 * Will only work for for either of the two lines below:
 * <li><a href="200.osc.gz">200.osc.gz</a></li>
 * <li><a href="../">../</a> (parent directory)</li>            <li><a href="214.osc.gz">214.osc.gz</a></li>
 **************************************************************/
int parseLI (DLIST *dstList, char *line){

	char		*startTag, *endTag;
	char		*attTag, *attEnd;
	char		*myLine;
	char		*ptr1, *ptr2;
	int		length;
	int		result;
	char		*filename;
	char		buf[MAX_NAME_LENGTH] = {0};

	ASSERTARGS (dstList && line);

	/* I assume 'list item' start and end tags are on the same line! */
	startTag = strstr(line, LIST_ITEM_TAG);
	if( ! startTag )

		return ztInvalidArg;

	endTag = strstr(line, LIST_ITEM_END);
	if( ! endTag )

		return ztInvalidArg;

	attTag = strstr(line, ATTRIBUTE_TAG);
	if( ! attTag )

		return ztInvalidArg;

	attEnd = strstr(line, ATTRIBUTE_END);
	if( ! attEnd)

		return ztInvalidArg;

	myLine = strdup(line);
	while (startTag) {

//printf("parseLI(): TOP OF LOOP @@@ myLine : <%s>\n", myLine);

		// move just after <li> tag
		myLine = myLine + strlen(LIST_ITEM_TAG);

		ptr1 = strchr(myLine, '>');
		ptr1++;
		ptr2 = strstr(myLine, ATTRIBUTE_END);
		if (!(ptr1 && ptr2))

			return ztInvalidArg;

		length = ptr2 - ptr1;

		sprintf(buf, "%.*s", length, ptr1);
		buf[length] = '\0';

		filename = (char*) malloc(length * sizeof(char));
		if ( ! filename ){
			fprintf (stderr, "%s: Error allocating memory in parseLI().\n", progName);
			return ztMemoryAllocate;
		}
		strcpy(filename, buf);

//printf("parseLI(): extracted filename is: <%s>\n", filename);

		if (filename[0] != '.') { /* ignore parent directory */

			result = ListInsertInOrder(dstList, filename);
			if (result != ztSuccess) {
				fprintf(stderr,
						"%s: Error returned from ListInsertInOrder().\n",
						progName);
				return result;
			}
		}

		startTag = strstr(myLine, LIST_ITEM_TAG);
		myLine = startTag;

	} /* end while(startTag) */

	return ztSuccess;

} /* END parseLI() */


/* I assume '<tr>' table row start and end tags are on the same line! Will only work for the one single line below 203 characters long:
<tr><td valign="top"><img src="/icons/compressed.gif" alt="[   ]"></td><td><a href="184.osc.gz">184.osc.gz</a></td><td align="right">2021-12-15 00:53  </td><td align="right">199K</td><td>&nbsp;</td></tr>
***********************************************************************************/

int parseTablerow (DLIST *dstList, char *line){

	char		*startTag, *endTag;
	char		*attTag, *attEnd;
	char		*tdStart, *tdEnd;
	char		*tHeaderEnd;
	char		*ptr1, *ptr2;
	int		length;
	int		result;
	char		*filename;
	char		buf[MAX_NAME_LENGTH] = {0};

	ASSERTARGS (dstList && line);

	tHeaderEnd = strstr(line, THEADER_END); /* skip table header line */
	if (tHeaderEnd)

		return ztSuccess;

	startTag = strstr(line, TABLE_ROW);
	if( ! startTag )

		return ztInvalidArg;

	endTag = strstr(line, TROW_END);
	if( ! endTag )

		return ztInvalidArg;

	/* we have '<td valign=....' AND '<td><a ...'
	 * our tdStart is the second one <td> */
	tdStart = strstr(line, TABLE_DATA);
	if ( ! tdStart )

		return ztInvalidArg;

	tdEnd = strstr(line, TDATA_END);
	if ( ! tdEnd )

		return ztInvalidArg;

	/* table row has an inner '<a' start & end tags */
	attTag = strstr(line, ATTRIBUTE_TAG);
	if( ! attTag )

		return ztInvalidArg;

	attEnd = strstr(line, ATTRIBUTE_END);
	if( ! attEnd)

		return ztInvalidArg;

	tdStart = tdStart + strlen(TABLE_DATA);

	ptr1 = strchr(tdStart, '>');
	ptr1++;
	ptr2 = strstr(tdStart, ATTRIBUTE_END);

	if (!(ptr1 && ptr2))

		return ztInvalidArg;

	length = ptr2 - ptr1;

	sprintf(buf, "%.*s", length, ptr1);
	buf[length] = '\0';

	filename = (char*) malloc(length * sizeof(char));
	if ( ! filename ){
		fprintf (stderr, "%s: Error allocating memory in parseTablerow().\n", progName);
		return ztMemoryAllocate;
	}
	strcpy(filename, buf);

	if (filename[0] != 'P') { /* ignore parent directory */

		result = ListInsertInOrder(dstList, filename);
		if (result != ztSuccess) {
			fprintf(stderr, "%s: Error returned from ListInsertInOrder().\n", progName);
			return result;
		}
	}

	return ztSuccess;

} /* END parseTablerow () */

/* find_strDL () : find element in double linked list of LINE_INFO pointer
 * that starts with string */

static ELEM *find_strDL (DLIST *src, char *string){

	ELEM *elem;
	ELEM *ret = NULL;
	LINE_INFO *lineInfo;
	char *line;
	int	found = 0;

	ASSERTARGS (src && string);

	elem = DL_HEAD (src);
	while (elem){

		lineInfo = (LINE_INFO *) elem->data;
		line = (char *) lineInfo->string;

		if (strncmp (line, string, strlen(string)) == 0){
			found = 1;
			break;
		}

		elem = DL_NEXT(elem);
	}

	if (found)
		ret = elem;

	return ret;
}
