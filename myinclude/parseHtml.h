/*
 * parsePage.h
 *
 *  Created on: Feb 19, 2022
 *      Author: wael
 */

#ifndef PARSEHTML_H_
#define PARSEHTML_H_

#define HTML_TAG		"<html>"
#define HEAD_TAG		"<head>"
#define META_TAG		"<meta"
#define TITLE_TAG		"<title>"
#define BODY_TAG		"<body>"
#define HEADER1_TAG "<h1>"
#define UNSORTED_LIST_TAG "<ul>"
#define LIST_ITEM_TAG "<li>"
#define ATTRIBUTE_TAG  "<a"
#define TABLE_TAG  "<table>"
#define TABLE_ROW  "<tr>"
#define TABLE_DATA  "<td>"
#define TABLE_HEADER  "<th>"


#define HTML_END	"</html>"
#define HEAD_END	"</head>"
#define META_END	">"
#define TITLE_END	"</title>"
#define BODY_END	"</body>"
#define HEADER1_END  "</h1>"
#define UNSORTED_LIST_END "</ul>"
#define LIST_ITEM_END "</li>"
#define ATTRIBUTE_END  "</a>"
#define INFO_TAG_START  "<!doctype"
#define TABLE_END  "</table>"
#define TROW_END  "</tr>"
#define TDATA_END  "</td>"
#define THEADER_END "</th>"


#include "dList.h"

int parseIndexPage (DL_LIST *list, char *filename);

int parseListItem(DL_LIST *dst, char	*line);

int isHtmlFileList (DL_LIST *list);

int parseByTag (DL_LIST *dstList, DL_ELEM *start,
		                     int (*parseFunc) (DL_LIST *dstList, char *string));

int parseTablerow (DL_LIST *dstList, char *line);

int parseLI (DL_LIST *dstList, char *line);

#endif /* PARSEHTML_H_ */
