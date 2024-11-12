/*
 * parse.h
 *
 *  Created on: Apr 14, 2023
 *      Author: wael
 */

#ifndef PARSE_H_
#define PARSE_H_

int parseCmdLine(MY_SETTING *ptrSetting, int argc, char* const argv[]);

int parseTimestampLine(struct tm *tmStruct, char *timeString);

int parseSequenceLine(char **sequenceString, const char *line);


#endif /* PARSE_H_ */
