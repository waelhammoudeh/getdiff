/*
 * parseAnchor.h
 *
 *  Created on: Jun 18, 2023
 *      Author: wael
 */

#ifndef PARSEANCHOR_H_
#define PARSEANCHOR_H_

#define HTML_TAG	"<html>"
#define HTML_END	"</html>"

#define ANCHOR_TAG  "<a"
#define ANCHOR_END  "</a>"


int isHtmlStringList(STRING_LIST *list);

int parseHtmlFile(STRING_LIST *destList, const char *filename);

int parseAnchor(STRING_LIST *destList, char *line);


#endif /* PARSEANCHOR_H_ */
