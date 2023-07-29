/*
 * ztError.h
 *
 *  Created on: Apr 15, 2023
 *      Author: wael
 */

#ifndef ZTERROR_H_INCLUDED
#define ZTERROR_H_INCLUDED

/* return enumerated types codes */
typedef enum ZT_EXIT_CODE_ {

  ztSuccess = 0,
  ztMissingArg,
  ztInvalidArg,
  ztUnknownOption,
  ztOptionMissingArg,
  ztMalformedCML,
  ztStringUnknown,
  ztEmptyString,

  ztParseError,
  ztDisallowedChar,

  ztConfInvalidKey,
  ztConfDupKey,
  ztConfUnregonizedKey,
  ztConfInvalidValue,
  ztConfBadKeyString,
  ztConfInvalidArray,
  ztConfInvalidType,

  ztOpenFileError,
  ztFileNotFound,
  ztNotRegFile,
  ztFileEmpty,
  ztUnexpectedEOF,
  ztEndOfFile,
  ztFileError,
  ztNoLinefeed,
  ztWriteError,
  ztMalformedFile,

  ztFnameLong,
  ztFnameDisallowed,
  ztFnameHyphen,
  ztFnameUnderscore,
  ztFnamePeriod,
  ztFnameMultiSlashes,
  ztFnameSlashEnd,

  ztStrNotPath,
  ztNoRelativePath,
  ztPathNotDir,
  ztInaccessibleDir,
  ztInaccessibleFile,
  ztNotExecutableFile,
  ztNoReadPerm,
  ztFailedSysCall,
  ztChildProcessFailed,
  ztFailedLibCall,

  ztFailedDownload,
  ztBadSizeDownload,

  ztMemoryAllocate,
  ztListEmpty,
  ztListNotEmpty,

  ztNoConnNet,
  ztNoConnDB,

  ztResponse301,

  ztResponse400,
  ztResponse403,
  ztResponse404,
  ztResponse429,
  ztResponse500,
  ztResponse503,
  ztResponseUnknown,
  ztResponseUnhandled,

  ztNotCookieFile,
  ztNoCookieToken,

  ztNoSession,
  ztOldCurl,
  ztQuerySyntax,
  ztNoNodesFound,
  ztNoGeometryFound,
  ztBadSegment,
  ztUndefinedSlope,

  ztFatalError,

  ztUnknownError,
  ztUnknownCode /* LAST ERROR */

} ZT_EXIT_CODE;

#define MAX_ERROR_CODE ztUnknownCode

typedef struct ZT_ERROR_ENTRY_{

  ZT_EXIT_CODE    errorCode;
  char    *errString;
  char    *description;

} ZT_ERROR_ENTRY;


char* ztCode2Msg(int code);


#endif /* ZTERROR_H_ */
