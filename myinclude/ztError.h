/*
 * ztError.h
 *
 *  Created on: Jun 26, 2017
 *      Author: wael
 */

#ifndef ZTERROR_H_
#define ZTERROR_H_

/* return enumerated types codes */
typedef enum {
	ztSuccess,
	ztMissingArgError,
	ztSystemGetcwdError,
	ztUnknownOption,
	ztMultipleFunctions,
	ztMissingOptionArg,
	ztInvalidArg,
	ztInaccessibleDir,
	ztInvalidUsage,
	ztNotFound,
	ztPathNotDir,
	ztBadFileName,
	ztNoReadPerm,
	ztNotRegFile,
	ztNullArg,
	ztOpenFileError,
	ztUnexpectedEOF,
	ztMissFormatFile,
	ztOutOfRangeDim,
	ztMemoryAllocate,
	ztNullPointer,
	ztOutOfRangePara,
	ztBadParam,
	ztInvalidFileEntry,
	ztListEmpty,
	ztRemoveNextToTail,
	ztMissMatchRow,
	ztListNotEmpty,
	ztErrorTreeFunc,
	ztWriteError,
	ztBadFileHeader,
	ztListOperationFailure,
	ztReadError,
	ztFileEmpty,
	ztFailedSysCall,
	ztMalformedCmndLine,
	ztFatalError,
	ztNonMerge,
	ztParseError,
	ztParseNoIP,
	ztNoConnError,
	ztDBname2long,
	ztDBnameStartEr,
	ztDBnameBadChr,
	ztInvalidConf,
	ztBadHelpArg,
	ztUpdateSettingError,
	ztInvalidBoolArg,
	ztLineLengthError,
	ztDisallowedChar,
	ztBadLineZI,
	ztStrTooLong,
	ztStrTooShort,
	ztGotNull,
	ztInvalidToken,
	ztChildProcessFailed,
	ztCreateFileErr,
	ztCompareNotSet,
	ztSmallBuf,
	ztInvalidResponse,
	ztFileNotFound,
	ztEmptyString,
	ztUnrecognizedMsg,
	ztBadResponse,
	ztHighResponse,
	ztPyExecNotFound,
	ztUnusableFile,
	ztOutResource,
	ztResponse403,
	ztResponse429,
	ztResponse500,
	ztResponseUnhandled,
	ztResponseUnknown,

	ztUnknownError,
	ztUnknownCode /* LAST ERROR */

} ztExitCodeType;

#define MAX_ERR_CODE ztUnknownCode

char* code2Msg(int code);


#endif /* ZTERROR_H_ */
