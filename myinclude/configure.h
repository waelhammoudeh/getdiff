#ifndef CONFIGURE_H_
#define CONFIGURE_H_

#ifndef DLIST_H_
#include "list.h"
#endif

extern int myFgetsError;
extern int confErrLineNum;

#define SPACE '\040'
#define TAB '\t'
#define LINEFEED '\n'
#define LINEBREAK '\r'
#define ZERO '\0'

#define MAX_KEY_LENGTH 16

#ifndef MAX_USER_NAME
#define MAX_USER_NAME 64
#endif

/* CLINE_MAX : maximum configure line length **/
#define CLINE_MAX 128


typedef enum CONF_TYPE_ {

  FILE_CT= 1,
  DIR_CT,
  INET_URL_CT,
  PORT_CT,
  NAME_CT,
  BOOL_CT,
  NONE_CT,
  DIGITS9_CT, /* string of 9 digits maximum **/
  ANY_CT,
  INVALID_CT /* keep last **/

} CONF_TYPE;

typedef struct CT2STRING_ {
	CONF_TYPE ct;
	char str[16];
} CT2STRING;

/* Notes:
 * NAME_CT : tested for string length only using #define MAX_USER_NAME 128
 * NONE_CT: accepted values ['none', 'off'] case ignored
 * DIGITS3_CT:  valid set: [0-999]
 *
 * To add new _CT add a case to test the new type in setConfValue() function AND
 * a case in fprintCEntry() function.
 *
 ********************************************************************/

typedef struct CONF_ENTRY_ {

  char *key;
  char *value;
  CONF_TYPE ctype;
  int index; /* index of this entry in an array of configure entries **/

} CONF_ENTRY;

typedef struct LINE_INFO_ {
  char		*string;
  int		originalNum;
} LINE_INFO;

#define OK_CONF_TYPE(t) ((t) > 0 && (t) < INVALID_CT)

int initialConf(CONF_ENTRY *entries, int numEntries);

int isOkayKey(char const *key);

void fprintCEntry(FILE *toFile, CONF_ENTRY *entry);

void fprintCArray(FILE *toF, CONF_ENTRY *array, int numEntries);

int boolStr2Integer(int *dest, char const *str);

char* myFgets(char *destStr, int count, FILE *fPtr, int *lineNum);

void zapLineInfo(void **data);

void fprintLineInfo(FILE *toPtr, LINE_INFO *lineInfo);

void fprintLineInfoList(FILE *dstFP, DLIST *list);

CONF_ENTRY *findEntryByKey(char const *key, CONF_ENTRY *confArray);

int setConfValue(CONF_ENTRY *entry, char const *value);

int configureGetValues(CONF_ENTRY *ceArray, int *numFound, char *confFile);

int isOkayBoolValue(const char *value);

char *myfgets(char **destStr, int *lineNumber, FILE *filePtr);

int file2LineInfoList(DLIST *list, char const *filename);

char *ct2Str(CONF_TYPE ct);

#endif /*END CONFIGURE_H_ **/
