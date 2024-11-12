#ifndef DEBUG_H_
#define DEBUG_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "getdiff.h"

void fprintPathPart(FILE *file, PATH_PART *pp);

void fprintStateInfo(FILE *toStream, STATE_INFO *si);

int logMessage(FILE *to, char *msg);

void fprintSkeleton(FILE *toFP, SKELETON *skl);

void fprintGdFiles(FILE *toFP, GD_FILES *gdfiles);

void fprintSetting(FILE *toFP, MY_SETTING *settings);


#endif /* DEBUG_H_ */
