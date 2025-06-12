#ifndef GD_PRIMITIVES_H_
#define GD_PRIMITIVES_H_

#include <time.h>

typedef struct SKELETON_ {

  char *workDir;
  char *tmp;

  char *geofabrik;
  char *planet;
  char *planetMin;
  char *planetHour;
  char *planetDay;

} SKELETON;

typedef struct GD_FILES_ { // get differ files

  // non-temporary files
  char *lockFile;
  char *logFile;
  char *previousSeqFile; // ID; sequence number
  char *prevStateFile;   // member to be removed
  char *newDiffersFile;
  char *rangeFile;

  // temporary files
  char *latestStateFile;

} GD_FILES;

typedef struct MY_SETTING_ {

  char *source;
  char *rootWD;
  char *configureFile;
  char *usr;
  char *pswd;
  char *startNumber;
  char *endNumber;

  int verbose;
  int newDifferOff;

  int textOnly;

} MY_SETTING;

typedef struct URL_PARTS_ { // not used?
	char scheme[8];
	char server[64];
	char dpage[64]; // differs page (where latest state.txt file is found)
} URL_PARTS;

typedef struct PATH_PART_ {

  char sequenceNum[10];

  char rootEntry[5];  /* 3 characters (digits) + ending slash character
                         for both rootEntry & parentEntry. **/
  char parentEntry[5];
  char fileEntry[4];  /* fileEntry does NOT end with slash **/

  char parentPath[10]; /* Parent Page / differs directory listing **/
  char filePath[16];

} PATH_PART;

typedef struct STATE_INFO_ {

  char timeString[24];
  char seqNumStr[10];
  char originalSeqStr[10];

  int  isGeofabrik;

  PATH_PART *pathPart;
  struct tm *timestampTM;
  time_t    timeValue;

} STATE_INFO;

int setupFilesys(SKELETON *directories, GD_FILES *files, const char *root);

int buildDirectories(SKELETON *dir, const char *where);

int setFilenames(GD_FILES *gdFiles, SKELETON *dir);

PATH_PART *initialPathPart(void);

void zapPathPart(void **pathPart);

STATE_INFO *initialStateInfo();

void zapStateInfo(STATE_INFO **si);

int stateFile2StateInfo(STATE_INFO *stateInfo, const char *filename);

char *stateFile2SequenceString(const char *filename);

int sequence2PathPart(PATH_PART *pathPart, const char *sequenceStr);

int isGoodSequenceString(const char *string);

int makeOsmDir(PATH_PART *startPP, PATH_PART *latestPP, const char *rootDir);

void zapSetting(MY_SETTING *settings);

void zapSkeleton(SKELETON *skel);

void zapGd_files(GD_FILES *gf);

#endif /* GD_PRIMITIVES_H_ **/
