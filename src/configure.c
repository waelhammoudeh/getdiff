/*
 * configure.c
 *
 *  Created on: Dec 18, 2018
 *      Author: wael
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configure.h"
// #include "osm2si.h"
#include "util.h"
#include "ztError.h"
#include "fileio.h"
#include "list.h"
// #include "parse.h" //for arg2Boolean()

/* readConfigure(): reads and parse configuration file. File is a text
 * file with each line starting with entry name followed by space or tab
 * and an option equal sign - sign maybe surrounded by white space, then
 * the value for the named entry only. It is an error to have anything
 * else on the the line.
 * confEntry is a pointer to an array of CONF_ENTRY with last member of
 * the array all zero or NULL. Configuration file is searched for the name
 * member of each entry, when name is matched, and value is the only
 * remaining string on the configuration line, value is tested according
 * to type member of CONF_ENTRY. If test is successful then value is set
 * in value member of CONF_ENTRY and numFound is incremented.
 * Function returns ztSuccess on success or ztInvalidConf on error, exported
 * variable confLN is set to line number in configuration file in the later
 * case.
 * infile is assumed to exist, we do not check for it.
 * Note that configuration file maybe empty or all commented out, function will
 * return success, check numFound.
 * numFound : number of read / found entries in file.
 */

int	confLN; // line number with error

int readConfigure(CONF_ENTRY *confEntry, int *numFound, char *infile){

	DLIST		*cfList;
	ELEM		*elem;
	LINE_INFO	*lineInfo;
	char		*entryName;
	char		*entryValue;
	char		*chkTkn; // check token
	char		*myCpy;
	char		*delimiters = "=\040\t"; // set [= SPACE and TAB]
	int			result;
//	int	confLN; //had to make global
	CONF_ENTRY	*arMover;

	ASSERTARGS (confEntry && numFound && infile); //abort() if we get null

	cfList = (DLIST *) malloc(sizeof(DLIST));
	if (cfList == NULL){
		printf("readConfigure(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}

	initialDL (cfList, zapLineInfo, NULL);

	result = file2List(cfList, infile);
	if (result != ztSuccess){
		printf("readConfigure(): Error returned by file2List()!\n");
		return result;
	}

	// if list is empty, nothing to do
	if (DL_SIZE(cfList) == 0){
		destroyDL(cfList);
		free(cfList);
		return ztSuccess;
	}

	/* the member "value" is used as sentinel when updating setting,
	 * set ALL value members to NULL */
	arMover = confEntry;
	while(arMover->name){

		arMover->value = NULL;
		arMover++;
	}

	elem = DL_HEAD(cfList);
	while (elem){

		lineInfo = (LINE_INFO *) elem->data;

		confLN = lineInfo->originalNum; //set exported line number
		chkTkn = NULL;

		myCpy = strdup(lineInfo->string);

		/* parse configure line; line consist of 2 tokens only. */
		entryName = strtok(myCpy, delimiters);
		entryValue = strtok(NULL, delimiters);
		if (entryValue)
			chkTkn = strtok(NULL, "\040\t");
		/* entryName and entryValue are the only allowed tokens
		 * it is an error if we find a third token on a line */

		if (chkTkn != NULL){ // found third token
			destroyDL(cfList);
			free(cfList);
			return ztInvalidConf;
		}

		// is entry in the array
		arMover = confEntry;
		while(arMover->name){

			if (strcmp(entryName, arMover->name) == 0){
				(*numFound)++;
				break;
			}

			arMover++;
		}

		if (arMover->name == NULL){ // did not find a match; it is an error
			destroyDL(cfList);
			free(cfList);
			return ztInvalidConf;
		}

		// check boolean values - RETHINK THIS TODO
		int iValue;
		if(arMover->type == BOOL_CT &&
		   (str2Boolean(&iValue, entryValue) != ztSuccess))

			return ztInvalidBoolArg;

		/* set/fill value member, no checking is done here --
		 * do NOT allow empty setting value! We allow missing setting ONLY.
		 * ***********************************************************/
		if (entryValue)

			arMover->value = strdup(entryValue);

		else {

			// cleanup, then
			return ztInvalidConf;
		}

		elem = DL_NEXT(elem);

	} //end while(elem)

	destroyDL(cfList);
	free(cfList);

	confLN = 0; // no errors were found, reset it.

	return ztSuccess;
}

/* checkPortString(): check port number as a string.
 * No character + within range.
 * Returns ztSuccess for good port number, ztInvalidArg otherwise. */
int checkPortString (char *num){

	char *endPtr;
	int  i;
	int  MaxPort = 65535;

	ASSERTARGS(num);

	i = strtol(num, &endPtr, 10);
	if (*endPtr != '\0') //has non digits

		return ztInvalidArg;

	if (i < 1 || i > MaxPort)

		return ztInvalidArg; //out of range

	return ztSuccess;
}

/* checkName_CT(): check name as configure type "CT".
 * max. length is 16 character, allowed are: [alphabets, digits - and _]
 * name must start with alphabet.
 * return ztSuccess for good name or ztInvalidArg for invalid name.
 */
int checkName_CT(char *name){

	char *digits = "0123456789";
	char *disallowed = "<>|,:()&;?*!@#$%^+=\040\t"; //TODO missing any??
		/* bad characters in file name, space and tab included */
	char hyphen = '-';
	char underscore = '_';
	int  maxLength = 16;

	ASSERTARGS(name);

	if (strlen(name) > maxLength)

		return ztInvalidArg;

	if (name[0] == hyphen || name[0] == underscore)
		//name can not start with hyphen or underscore

		return ztInvalidArg;

	if (strpbrk(name, digits) == name) // do not start with digit

		return ztInvalidArg;

	if (strpbrk(name, disallowed))

		return ztInvalidArg;

	return ztSuccess;
}

/* str2Boolean(): stores integer value of 1 or 0 in dest pointer after
 * testing character string pointed to by str;
 * 1 is stored if str is pointing at one of: on, true or 1.Case ignored.
 * 0 is stored if str is pointing at: off, false or 0. Case ignored.
 * Function returns ztSuccess if str is one of above strings.
 * It returns ztInvalidArg if not.
 **********************************************************************/

int str2Boolean(int *dest, char const *str){

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

} //END str2Boolean()

