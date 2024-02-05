/*
 * ztError.c
 *
 *  Created on: Apr 15, 2023
 *      Author: wael
 */

#include <stdio.h>
#include <string.h>

#include "ztError.h"

ZT_ERROR_ENTRY    ztErrorTable[] = {

  {ztSuccess,
   "ztSuccess",
   "Program terminated normally."},

  {ztMissingArg,
   "ztMissingArg",
   "Missing required argument."},

  {ztInvalidArg,
   "ztInvalidArg",
   "Invalid argument for specified option."},

  {ztUnknownOption,
   "ztUnknownOption",
   "Unknown option specified."},

  {ztOptionMissingArg,
   "ztOptionMissingArg",
   "specified option missing required argument."},

  {ztMalformedCML,
   "ztMalformedCML",
   "Malformed command line, extra argument(s) found. Please check syntax."},

  {ztStringUnknown,
   "ztStringUnknown",
   "Unknown string found, did not match what is expected."},

  {ztEmptyString,
   "ztEmptyString",
   "String with string length zero"},

  {ztNotString,
   "ztNotString",
   "Not a string; last character / byte not null character"
  },

  {ztStringNotFound,
   "ztStringNotFound",
   "String not found in list or file"
  },

  {ztMalformedStr,
   "ztMalofromedStr",
   "Malformed string, string was not to specification by parse function"
  },

  {ztInvalidLatValue,
   "ztInvalidLatValue",
   "Invalid LATITUDE value for Arizona",
  },

  {ztInvalidLonValue,
   "ztInvalidLonValue",
   "Invalid LONGITITUDE value for Arizona"
  },

  {ztInvalidGpsValue,
   "ztInvalidGpsValue",
   "Invalid GPS point for local range"
  },

  {ztInvalidBbox,
   "ztInvalidBbox",
   "Invalid bounding box; values are not in the correct order"
  },

  {ztParseError,
   "ztParseError",
   "Parse function failed. String was not as expected."},

  {ztDisallowedChar,
   "ztDisallowedChar",
   "Parsed string has disallowed character for that operation."},

  {ztConfInvalidKey,
   "ztConfInvalidKey",
   "Invalid Key name in configuration file; unrecognized key name."},

  {ztConfDupKey,
   "ztConfDupKey",
   "Found duplicate keys; duplicates are not allowed in configuration file."},

  {ztConfUnregonizedKey,
   "ztConfUnregonizedKey",
   "Unrecognized key: key in configuration file but NOT in initialed configure array" },

  {ztConfInvalidValue,
   "ztConfInvalidValue",
   "Invalid value for key in configuration file."},

  {ztConfBadKeyString,
   "ztConfBadKeyString",
   "Invalid string for key name, see allowed set"},

  {ztConfInvalidArray,
   "ztConfInvalidArray",
   "Configure invalid initial array parameter"},

  {ztConfInvalidType,
   "ztConfInvalidType",
   "Configure invalid configure type"},

  {ztOpenFileError,
   "ztOpenFileError",
   "Failed to open or create file."},

  {ztFileNotFound,
   "ztFileNotFound",
   "Specified file or directory does not exist."},

  {ztNotRegFile,
   "ztNotRegFile",
   "Argument is not a regular file - on disk or mass storage."},

  {ztFileEmpty,
   "ztFileEmpty",
   "Specified file is empty, has size zero." },

  {ztUnexpectedEOF,
   "ztUnexpectedEOF",
   "Unexpected end of file; function did not read all expected data."},

  {ztEndOfFile,
   "ztEndOfFile",
   "End of file is set to non-zero value."},

  {ztFileError,
   "ztFileError",
   "File operation error occured; file error is set."},

  {ztNoLinefeed,
   "ztNoLinefeed",
   "No linefeed character at end of string"},

  {ztWriteError,
   "ztWriteError",
   "Failed write operation, did not write whole string to file."
  },

  {ztMalformedFile,
   "ztMalformedFile",
   "Malformed file format; file contents are not as expected"
  },

  {ztFnameLong,
   "ztFnameLong",
   "Filename is longer than 255 characters or path string is too longer than 4096 characters."},

  {ztFnameDisallowed,
   "ztFnameDisallowed",
   "Name has disallowed character; allowed: [alphabets (upper & lower) -_.]"},

  {ztFnameHyphen,
   "ztFnameHyphen",
   "Hyphen character '-' can not be first or last in filename or path part."},

  {ztFnameUnderscore,
   "ztFnameUnderscore",
   "Underscore character '_' can not be first or last in filename or path part."},

  {ztFnamePeriod,
   "ztFnamePeriod",
   "Period character '.' can not be last in filename or path part."},

  {ztFnameMultiSlashes,
   "ztFnameMultiSlashes",
   "Double or multiple slashes are not allowed in path."},

  {ztFnameSlashEnd,
   "ztFnameSlashEnd",
   "Filename can not end with a slash character"},

  {ztStrNotPath,
   "ztStrNotPath",
   "String is not a valid absolute path - must start with a slash; filename is assumed 'absolute path + filename'!"
  },

  {ztNoRelativePath,
   "ztNoRelativePath",
   "Relative paths are not allowed, no path expansion or substitution is done"
  },

  {ztPathNotDir,
   "ztPathNotDir",
   "Specified path is not a directory in the file system."},

  {ztInaccessibleDir,
   "ztInaccessibleDir",
   "Inaccessible directory, missing at least one permission [Read, Write, eXecute]"},

  {ztInaccessibleFile,
   "ztInaccessibleFile",
   "Inaccessible file, missing at least one permission [Read, Write, eXecute]"},

  {ztNotExecutableFile,
   "ztNotExecutableFile",
   "File is NOT executable by the effective user"},

  {ztNoReadPerm,
   "ztNoReadPerm",
   "File or directory missing required read permission."},

  {ztFailedSysCall,
   "ztFailedSysCall",
   "Failed system call function."},

  {ztChildProcessFailed,
   "ztChildProcessFailed",
   "Child process terminated with an error."},

  {ztFailedLibCall,
   "ztFailedLibCall",
   "Failed external library call or function."},

  {ztFailedDownload,
   "ztFailedDownload",
   "Failed curl_easy_perform() curl library function; (result != CURLE_OK)"},

  {ztBadSizeDownload,
   "ztBadSizeDownload",
   "Failed size test for download; file disk size did not match sizeHeader or sizeDownload"},

  {ztMemoryAllocate,
   "ztMemoryAllocate",
   "Memory allocation failure. Failed to allocate requested memory."},

  {ztListEmpty,
   "ztListEmpty",
   "List is empty - list size is zero - while function expects non-zero size."},

  {ztListNotEmpty,
   "ztListNotEmpty",
   "List is NOT empty - list size is NOT zero - while function expects zero size list."},

  {ztNoConnNet,
   "ztNoConnNet",
   "No network or internet connection."},

  {ztNetConnFailed,
   "ztNetConnFailed",
   "Established network connection failure."
  },

  {ztHostResolveFailed,
   "ztHostResolveFailed",
   "Failed to resolve host name (curl)"
  },

  {ztNoConnDB,
   "ztNoConnDB",
   "Failed to connect to database server."},

  {ztResponse200,
   "ztResponse200",
   "Server response was 'Okay', does not mean we got what we wanted."
  },

  {ztResponseFailed2Retrieve,
   "ztResponseFailed2Retrieve",
   "Function failed to retrieve server response code"
  },

  {ztResponseNone,
   "ztResponseNone",
   "Server response was NOT set"
  },

  {ztResponse301,
   "ztResponse301",
   "Server response code 301: Requested resource Moved Permanently"},

  {ztResponse302,
   "ztResponse302",
   "Moved is only relevant in the context of the permanent id feature. Says overpass.)"
  },

  {ztResponse400,
   "ztResponse400",
   "Server response code 400: [Bad Request] Server did not understand us, query syntax error."},

  {ztResponse403,
   "ztResponse403",
   "Server response code 403: [Forbidden] Invalid credential; wrong user name or password."},

  {ztResponse404,
   "ztResponse404",
   "Server response code 404: [Not Found] Requested resource (file) was not found by this server."},

  {ztResponse429,
   "ztResponse429",
   "Server response code 429: [Too Many Requests] Too many downloads in short time. Multiple queries from one IP."},

  {ztResponse500,
   "ztResponse500",
   "Server response code 500: [Internal Server Error] Server is busy or overpass dispatcher is not running."},

  {ztResponse502,
   "ztResponse502",
   "Server response code 502: [Bad Gateway Error] while acting as a gateway proxy."},

  {ztResponse503,
   "ztResponse503",
   "Server response code 503: [Server Unavailable Now] Server is not available now; overloaded?"},

  {ztResponse504,
   "ztresponse504",
   "Gateway Time: Overloaded overpass server. May also be large query setting for timeout or maxsize"
  },

  {ztResponseUnknown,
   "ztResponseUnknown",
   "Server response code is not known to us. Failed to retrieve server response code!"},

  {ztResponseUnhandled,
   "ztResponseUnhandled",
   "Server response code is NOT handled by this program, add a case for it."},

  {ztNotCookieFile,
   "ztNotCookieFile",
   "File is not Geofabrik.de cookie file."},

  {ztNoCookieToken,
   "ztNoCookieToken",
   "Missing cookie token, failed getCookieToken() with NULL result"},

  {ztNoCurlSession,
   "ztNoCurlSession",
   "Curl session was not initialed; function requires initialCurlSession() call first."},

  {ztOldCurl,
   "ztOldCurl",
   "Old curl library version found. Please check minimum required version."},

  {ztQuerySyntaxError,
   "ztQuerySyntaxError",
   "Query syntax error reported by server with response code 400."},

  {ztNoNodesFound,
   "ztNoNodesFound",
   "Function query4Crossing() may return this with zero nodes returned from server."},

  {ztNoGeometryFound,
   "ztNoGeometryFound",
   "Function query4Path() may return this value when server response has zero geometry."},

  {ztBadSegment,
   "ztBadSegment",
   "Malformed SEGMENT with one single point."},

  {ztUndefinedSlope,
   "ztUndefinedSlope",
   "Undefined slope: this IS to avoid division by zero. Check longitude difference between points."},

  {ztFatalError,
   "ztFatalError",
   "Program or function encountered a fatal error; could not continue and terminates." },

  {ztNoQuerySession,
   "ztNoQuerySession",
   "Query session was not initialed; function requires qInitialSession() call first."},

  {ztCurlTimeout,
   "ztCurlTimeout",
   "Curl operation exceeded set timeout value - non-zero."
  },

  {ztUnknownError,
   "ztUnknownError",
   "Error: Unknown error."},

  {ztUnknownCode,
   "ztUnknownCode",
   "Error: Unknown error CODE specified!"},

  {-1777,
   NULL,
   NULL}

};


char* ztCode2Msg(int code){

  static char *msg;

  if (code < 0 || code > MAX_ERROR_CODE){

    msg = strdup(ztErrorTable[MAX_ERROR_CODE].description);
    return msg;
  }

  msg = strdup(ztErrorTable[code].description);

  return msg;

} /* END ztCode2Msg() **/

char* ztCode2ErrorStr(int code){

  static char *errStr;

  if (code < 0 || code > MAX_ERROR_CODE){

    errStr = strdup(ztErrorTable[MAX_ERROR_CODE].errString);
    return errStr;
  }

  errStr = strdup(ztErrorTable[code].errString);

  return errStr;

} /* END ztCode2ErrorStr() **/
