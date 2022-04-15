/*
 * configure.h
 *
 *  Created on: Dec 18, 2018
 *      Author: wael
 */

#ifndef CONFIGURE_H_
#define CONFIGURE_H_

/* CONF_TYPE type / kind of entry used for checking value as string.
 * Note that type here has nothing to do with c language types.
 *******************************************************************/
typedef enum CONF_TYPE_ {
	FILE_CT= 1,
	DIR_CT,
	URL_CT,
	PORT_CT,
	NAME_CT, /* max. 16 character, set: alphanumeric, minus sign and underscore
			  * starts only with alphabet */
	BOOL_CT,
	ANY_CT

} CONF_TYPE;

typedef struct CONF_ENTRY_ {

	const 	char 		*name;
			char 		*value;
			CONF_TYPE 	type;
			int const	index;

} CONF_ENTRY;

extern int	confLN;

int readConfigure(CONF_ENTRY *confEntry, int *numFound, char *infile);

int checkPortString (char *num);

int checkName_CT(char *name);

int str2Boolean(int *dest, char const *str);

#endif /* SOURCE_CONFIGURE_H_ */
