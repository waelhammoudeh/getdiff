/*
 * ztError.c
 *
 *  Created on: Jun 26, 2017
 *      Author: wael
 */

#include <stdio.h>

#include "ztError.h"


static char *errorString[] = {
		"Program terminated normally",
		"Error: no argument provided for program, requires at least one argument.",
		"Error: system call getcwd() failed!",
		"Error: Unknown option specified.",
		"Error: Multiple function specified.",
		"Error: Missing argument for specified option",
		"Error: Invalid argument for specified option",
		"Error: Inaccessible directory, missing at least one permission [R, W, X]",
		"Error: Invalid usage of option with specified command",
		"Error: Specified file or directory does not exist.",
		"Error: Directory entry is required, provided path is not a directory",
		"Error: Bad file name, has disallowed character, starts with digit or hyphen",
		"Error: Missing required read permission",
		"Error: Argument is not a regular file.",
		"Error: NULL pointer argument - at least one.",
		"Error: Open text file for reading",
		"Error: Unexpected end of file; did not read all expected data",
		"Error: Miss formatted input text file.",
		"Error: Read row or column dimension is out of range.",
		"Error: Could not allocate requested memory size.",
		"Error: Null pointer error, accessing uninitialized pointer.",
		"Error: Out of range function parameter, usually an integer",
		"Error: Bad parameter value for function.",
		"Error: Invalid input file entry, bad data token",
		"Error: Attempting to remove from an EMPTY list",
		"Error: Attempting to remove element next to tail",
		"Error: Miss match row latitude values",
		"Error: Parameter list is NOT empty; function expects an empty list",
		"Error: Binary Tree function returned error .. insert operation failed!",
		"Error: fwrite error, result does not match sizeof buffer",
		"Error: Bad file header in data file",
		"Error: List operation failure",
		"Error: fread error result does not match sizeof buffer",
		"Error: Specified file is empty",
		"Error: Failed system call.",
		"Error: Malformed command line, extra argument(s) found.",
		"Error: Fatal error, major error ...",
		"Error: Tree files are not mergeable.",
		"Error: Parse function failed. Error with string!?",
		"Error: Could not retrieve IP number for specified server, spelling?",
		"Error: Unable to connect to server provided.",
		"Error: Database name or user too long. MaxLength = 16",
		"Error: Database can not start with digit, - or _.",
		"Error: Database name has disallowed character.",
		"Error: Invalid configuration line; extra token, unrecognized entry name or empty setting.",
		"Error: Bad - unrecognized help argument; valid arg = [zoneinfo | configure]",
		"Error: updateSettings() miss matched number of array entries",
		"Error: Invalid argument for boolean value.",
		"Error: String line length out of checked range",
		"Error: String has disallowed character - parse error!",
		"Error: Bad zone info line; out of range number of tokens",
		"Error: String or line is too long. Function dependent",
		"Error: String or line is too short. Function dependent",
		"Error: A library or system function call returned null.",
		"Error: Parse function encountered invalid token",
		"Error: Child process from fork() failed.",
		"Error: Failed to create file for writing.",
		"Error: Pointer to function compare not set with initialDL().",
		"Error: Small hard coded buffer size.",
		"Error: Invalid HTTP response code from remote server. Server may provide error message",
		"Error: Argument file was not found.",
		"Error: Empty string; string length is zero.",
		"Error: Unrecognized error message by parse function.",
		"Error: Bad response code, not integer, code includes non-digit character.",
		"Error: High HTTP response code, higher than 599.",
		"Error: Python3 executable not found",
		"Error: Named file is unusable file; failed IsArgUsableFile() call.",
		"Error: system run out of a resource; memory or full disk.",
		"Error: Received server response code 403; invalid credential",
		"Error: Received server response code 429; too many requests",
		"Error: Received server response code 500; internal server error",
		"Error: Unhandled HTTP response code by program",
		"Error: Unknown response code; response was not received."

		"Error: Unknown error.",
		"Error: Unknown error CODE specified!"
};


char* code2Msg(int code){

	char *msg;

	if (code < 0 || code > MAX_ERR_CODE){
		msg = errorString[MAX_ERR_CODE];
		return msg;
	}

	msg = errorString[code];

	return msg;
}



