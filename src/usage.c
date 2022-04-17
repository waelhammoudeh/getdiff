/*
 * usage.c
 *
 *  Created on: Feb 10, 2022
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>

#include "getdiff.h"
#include "ztError.h"

#define STYLE_BOLD         "\033[1m"
#define STYLE_NO_BOLD   "\033[22m"

void showHelp(void){

	char		*intro = "%s: program to download OSM (OpenStreetMap) differ files for a specified\n"
							  "region (area) from \"www.geofabrik.de\" web site public or internal servers.\n\n"
			"Geofabrik OSM differ files (.osc) are generated daily, they are named with three digit\n"
			"numbers, there is no guarantee that those numbers are in sequence and each (.osc) file\n"
			"has a corresponding (.state.txt) file. This program downloads both differ (.osc) and\n"
			"its corresponding (.state.txt) files. For more information please see their description\n"
			"page: http://geofabrik.de/data/download.html\n"
			"This program does not update any data.\n\n";

	char		*noticeTemplate =
	"                        Please NOTE the following:\n"
	"Geofabrik GmbH generously provides many free services to OSM communities, this\n"
	"deserves our sincere and deep gratitude. Please do not overuse this program or\n"
	"abuse geofabrik.de servers. Please do not download too many files in a short\n"
	"period of time. Please familiarize yourself with geofabrik GmbH by visiting\n"
	"their web site : www.geofabrik.de.\n"
	"I also would like to thank each user for following the above advise and this\n"
	"program usage instructions - specifically know your start download file.\n\n";

	char		*cliDes =
	"Program accepts input from the command line, configuration file or combination\n"
	"of both. At least one option must be provided on the command line. Command line\n"
	"option over rides the same configuration settings. Default configuration file is used\n"
	"unless changed by user. Configuration file is a text file and described below.\n\n"

"Usage: %s [option optarg]\n\n"
			"   -h, --help          Show this information page.\n"
			"   -v, --verbose       Prints progress messages.\n"
			"   -s, --source URL    Specify full URL with area differ files list.\n"
			"   -d, --directory DIR Program work directory, default \"{USERHOME}/Downloads/getdiff/\".\n"
			"   -b, --begin NUM     Specify 3 digits differ file number to start download from.\n"
			"   -u, --user NAME     Specify OSM account user name.\n"
			"   -p, --passwd SECRET Specify OSM account password.\n"
			"   -c, --conf FILE     Configuration file to use, default /etc/getdiff.conf \n"
			"   -n, --new FILE      File (path + name) append list of downloaded file to.\n\n"

"Program accepts short or long option, each option must be followed by its\n"
"argument, except \"help and verbose\".\n"
"Options and their arguments are as follows:\n\n"

"--verbose: Turns verbose mode on, which is off by default.\n\n"

"--source URL: full path to area differ files list including geofabrik server\n"
"              name. This is the URL to your area used in browser to view\n"
"              available differ files. It includes the area name and ends with\n"
"              numbered directory entries /000/003/.\n\n"

"--directory DIR: This is the program work directory, must end in \"getdiff\" name.\n"
"                 \"getdiff\" is added to argument \"DIR\" if missing, log file and\n"
"                 other program files are saved to work directory. Program creates\n"
"                 \"diff\" directory under this work directory where downloads are\n"
"                 saved in. Default is \"{USERHOME}/Downloads/getdiff/\" where\n"
"                 {USERHOME} is current user home directory.\n\n"

"--begin NUM: The three digit number from differ file name to start download from.\n"
"             For initial use of program only, after first use, program writes and\n"
"             uses \"start_id\" file in its work directory and ignores this option.\n"
"             Each differ file has a corresponding state file which we download also.\n"
"             Please see \"Setting --begin NUM argument\" below.\n\n"

"--user NAME: OpenStreetMap account user name, required to access the internal\n"
"             server at \"https://osm-internal.download.geofabrik.de\". Name is\n"
"             limited to 64 character length for this program. This option is\n"
"             ignored when geofabrik public server is in use.\n\n"

"--passwd SECRET: The password for the above account in plain text. This option\n"
"                 is ignored when geofabrik public server is in use.\n\n"

"--conf FILE: The user configuration file to use. Default is /etc/getdiff.conf.\n"
"             If that is not found, program looks in its own executable directory\n"
"             for \"getdiff.conf\" as a second default alternative.\n\n"

"--new FILE: File is path and name where just downloaded files list is appended to.\n"
"            List is to be used by \"update script\", script should remove file\n"
"            when done.\n\n";

	char		*confDesc =
	"Configuration file is a text file with each line providing a directive pair of\n"
	"NAME and VALUE separated by space or tab and an optional equal sign '='.\n"
	"Comment lines start with '#' or ';' characters, no comment is allowed on a\n"
	"directive line. Directive names in configuration file are all capitalized, not\n"
	"to be changed and similar to command line options above. Unrecognized directive\n"
	"name triggers an error. Directive names are as follows: \n\n"

	" VERBOSE : accepted values {on, off, true, false, 1, 0} case ignored for value.\n\n"

	" SOURCE : same as --source option above.\n\n"

	" DIRECTORY : same as --directory option above.\n\n"

	" BEGIN : same as --begin option above.\n\n"

	" USER : same as --user option above.\n\n"

	" PASSWD : same as --passwd option above.\n\n"

	" NEWER_FILE : same as --new option above.\n\n";

	char const *confExample =

			"The following is an example configuration file:\n\n"
			"# This is getdiff.conf file; configuration file for getdiff program.\n"
			"# This line is a comment line.\n"
			"; So is this one too. No comment is allowed on directive line.\n\n"

			"# USER : OSM openstreetmap user name. email??\n"
			"# name is limited to 64 characters.\n"
			"USER = johndoe\n\n"

			"# PASSWD: password for OSM user.\n"
			"# PASSWD = mysecret\n\n"

			"# SOURCE: source with update file list on \"https://osm-internal.download.geofabrik.de\"\n"
			"or \"https://download.geofabrik.de\"\n"
			"# Try to always use \"https:\" NOT  \"http:\" with public server.\n"
			"# Full URL is required. https://osm-internal.download.geofabrik.de/ + {area_name}/{area_name}-updates/000/003/\n"
			"SOURCE = https://osm-internal.download.geofabrik.de/north-america/us/arizona-updates/000/003/\n\n"

			"# DIRECTORY : program working directory for the program.  A directory with\n"
			"# the name \"diff\" will be created under this for destination where update files will be\n"
			"# saved to. This has a default {$HOME}/Downloads/getdiff/, so you will find your\n"
			"# files in \"{$HOME}/Downloads/getdiff/diff/\" directory.\n"
			"# Full real path here. No path expansion is done. User has write permission on parent directory.\n"
			"# Directory is created if not present ONLY if parent directory exist. Path is used as entered.\n"

			"DIRECTORY = /path/to/your/getdiff/\n\n"

	"# BEGIN: start the download from file number - only the three digit number from file name.\n"
	"# This is for first use! Afterward, program reads its created file \"start_id\".\n"
	"BEGIN = 264\n\n"

	"# To turn verbose on use: TRUE, ON or 1. Case ignored for TRUE and ON.\n"
	"VERBOSE = True\n\n"

	"# Script helper! Update script should remove this file when done.\n"
	"# List of just downloaded files is added - appended - to this file.\n"
	"# New downloads recreate file again.\n"
	"NEWER_FILE = /path/to/newerFile.txt\n\n"

	"# End configuration example.\n\n";


	char		const *numArg =
	"Setting --begin NUM argument:\n\n"

	"NUM is the number part in the differ file name to start download from.\n\n"

	"First thing to know is the last date included in your OSM data file to update.\n"
	"The region download page on geofabrik.de site lists this date, for example:\n"
	"\"This file was last modified 1 day ago and contains all OSM data up to 2022-04-09T20:21:55Z. File size: 195 MB;\"\n"
	"This date can also be retrieved from the OSM data file with \"osmium fileinfo\"\n"
	"using the \"--extended\" option as:\n"
	" osmium fileinfo --extended regionfilename.osm.pbf\n\n"

	"note that regionfilename.osm.pbf can be any supported file type by osmium.\n\n"
	"Unlike OSM (.osc) change files, region files are not generated daily. This is another\n"
    "reason to use a tool like osmium.\n\n"

	"The second thing to know is that geofabrik.de keeps differ files for a period of\n"
	"three months. So if your data is older, you need more recent OSM data file for\n"
	"your region.\n\n"

	"Finally, each differ file has a corresponding state file containing time stamp\n"
	"and sequence number. This time stamp is the time the differ was generated. This\n"
	"is usually - not always - very close to last date for data contained in (.osc)\n"
	"differ file.\n\n"

	"Examine the date the time stamp line has, set NUM to file name number where this\n"
	"date is just before last date in your region OSM data file.\n\n"
	"%s"
	"Remember that this program does NOT update your data."
	"%s\n\n";

	char		const *howItworks =

	"The source URL is a required argument for the program to work. The server part in\n"
	"the source argument must be geofabrik public or internal server only. When internal\n"
	"server is in use; user name and password are required arguments.\n"
	"At least one argument must be provided on the command line - you are free to specify\n"
	"all on the command line. In that case the program does not even look for any\n"
	"configuration file.\n\n"
	"On startup, program sets configuration file to use. User specified file is used when found.\n"
	"It is an error when user specified configuration file is not found. Program uses default\n"
	"configuration file \"/etc/getdiff.conf\" if it exists and user does not specify one. If this\n"
    "default file is not found, program looks in its executable directory for \"getdiff.conf\" and\n"
	"uses that if it exist. Program prints out a warning message if no configuration file is set.\n"
    "Configuration file can not be empty, it does NOT need to provide any setting, it is an\n"
    "error to have any key name other than {VERBOSE, USER, PASSWD, DIRECTORY, BEGIN}.\n\n"

    "Program tests internet connection and terminates if that test fails.\n\n"

	"Program writes the following files in its work directory:\n\n"

	"geofabrikCookie.text : most recent geofabrik internal server cookie file.\n"
	"index.html : most recent HTML source for the source URL.\n"
	"indexList.txt : sorted list of extracted file names from index.html file.\n"
	"start_id : three digit number from last downloaded file name. DO NOT delete!\n"
	"getdiff.log : program logs.\n\n"

	"Typically one would store SOURCE, USER and DIRECTORY in a configuration file and\n"
	"start the program with :\n"
	" ~$ getdiff -c /path/to/my/getdiff.conf -p xxxxxxxx\n"
	" where xxxxxxxx is USER password for openstreetmap.org account.\n\n";

	fprintf(stdout, intro, progName);

	fprintf(stdout, "%s%s%s", STYLE_BOLD, noticeTemplate, STYLE_NO_BOLD);

	fprintf(stdout, cliDes, progName);

	fprintf(stdout, confDesc);

	fprintf(stdout, confExample);

	fprintf(stdout, numArg, STYLE_BOLD, STYLE_NO_BOLD);

	fprintf(stdout, howItworks);

	exit (ztSuccess);
}

void shortHelp(void){

	char		const *usage =
			"Usage: %s [option optarg]\n\n"
			"   -h, --help          Show help information page.\n"
			"   -v, --verbose       Prints progress messages.\n"
			"   -s, --source URL    Specify full URL with area differ files list.\n"
			"   -d, --directory DIR Program work directory, default \"{USERHOME}/Downloads/getdiff/\".\n"
			"   -b, --begin NUM     Specify 3 digits differ file number to start download from.\n"
			"   -u, --user NAME     Specify OSM account user name.\n"
			"   -p, --passwd SECRET Specify OSM account password.\n"
			"   -c, --conf FILE     Configuration file to use, default /etc/getdiff.conf \n\n";

	fprintf(stderr, usage, progName);
	fprintf(stdout, " Try: %s --help\n\n", progName);

	return;
}
