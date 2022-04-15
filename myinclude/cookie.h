/*
 * cookie.h
 *
 *  Created on: Feb 15, 2022
 *      Author: wael
 */

#ifndef COOKIE_H_
#define COOKIE_H_

/* geofabric cookie file NOW has only one single line - they may change this format */
#define MAX_COOKIE_LINES		1

typedef struct	COOKIE_ {

	char		*token;
	char		*expYear;
	int		year;
	char		*expMonth;
	int		month; // Jan = 0 - Dec = 11
	char		*expDayMonth;
	int		dayMonth; // 1 - 31
	char		*expDayWeek;	// day of the week Sun, Mon
	int		dayWeek; // Sun = 0, Mon = 1 ...
	char		*expHour;
	int		hour;
	char		*expMinute;
	int		minute;
	char		*expSecond;
	int		second;
	char		*format;
	char		*path;
	char		*sFlag;
	char		*expireTimeStr;

} COOKIE;

int writeScript (char	*fileName);

int writeJSONfile(char *filename, SETTINGS *settings);

int getCookieFile (SETTINGS *settings);

int parseCookieFile (COOKIE *dstCookie, char	*filename);

int parseTimeStr (COOKIE *cookie, char const *str);

int day2num (char *day);

int month2num (char *month);

void printCookie(COOKIE *ck);

int isExpiredCookie(COOKIE *ck);

int removeFiles (SETTINGS *settings);

#endif /* COOKIE_H_ */
