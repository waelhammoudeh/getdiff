/*
 * usage.c
 *
 *  Created on: Feb 10, 2022
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include "ztError.h"
#include "getdiff.h"
#include "usage.h"

void showHelp(void){

  char   *description =

    "Program \"%s\" is a command-line tool that allows users to download (OSM)\n"
    "Open Street Map differs; (.osc Open Street Change) files from a supported\n"
    "remote server listed below.\n"
    "This program is designed to be part of an automation process to update OSM\n"
    "database or data file, program does not update any data by itself.\n\n"

	"Supported servers are:\n"

    "  https://download.geofabrik.de/\n"
    "  https://osm-internal.download.geofabrik.de/\n"
    "  https://planet.osm.org/\n"
    "  https://planet.openstreetmap.org/\n\n"

    "The program supports a configuration file and accepts input from the command\n"
    "line, configuration file or combination of both. Command line setting overrides\n"
    "configuration file setting. Default configuration file is: {HOME}/getdiff.conf\n"
    "where {HOME} is current user home directory. You may specify a different file\n"
    "with '--conf' switch. Configuration file is a text file and is described below.\n\n"

    "Arguments required for long option are also required for short option.\n\n";

  char   *usage1 =

    "Usage: %s [OPTION]\n\n"
    "   -h, --help          Show this information page.\n"
    "   -V, --version       Prints program version and exits.\n"
    "   -v, --verbose       Prints progress messages to terminal. Default is off.\n"
    "   -s, --source URL    Set internet address URL to point to your region updates directory at\n"
    "                       geofabrik.de server; where the latest 'state.txt' file is found.\n"
    "   -d, --directory DIR Set the root directory for the program working directory; default is\n"
    "                       user {HOME} directory.\n"
    "   -b, --begin NUM     Specify sequence number for the change file to start downloading from.\n"
    "                       This argument is required for first time use and ignored afterward.\n"
    "   -e, --end NUM       Specify sequence number for change file to stop downloading at.\n"
    "                       With 'begin NUM' above set, this provides a range of change files to download\n"
    "                       When 'end' equals 'begin', that change file and its corresponding state.txt\n"
    "                       are downloaded.\n"
    "   -u, --user NAME     Specify OSM account user name. Required for geofabrik internal server.\n"
    "   -p, --passwd SECRET Specify the password for the account above.\n"
    "   -c, --conf FILE     Configuration file to use, default {HOME}/getdiff.conf \n"
    "   -n, --new           Specify the action for newly downloaded files. By default, the program\n"
    "                       appends the newly downloaded file names to 'newerFiles.txt'. Use this\n"
    "                       option to change this behavior.\n\n"

    "Arguments are explained below.\n\n";

  char   *usageURL =

    "URL for --source:\n"

	"Source argument is the full URL on the supported remote server where the latest\n"
	"'state.txt' file is found.\n"

    "For Geofabrik servers, the path starts with a continent name and ends with area or\n"
    "country name with '-updates' prepended to it.\n"
    "For Planet servers, the path starts with 'replication' entry followed by one of\n"
    "[minute | hour | day].\n"
    "In all cases this path is where the latest 'state.txt' file is found.\n\n"

    "Some examples for URL string are below, please verify your URL:\n\n"

    "https://download.geofabrik.de/africa/{area}-updates/\n"
    "https://download.geofabrik.de/antarctica-updates/\n"
    "https://download.geofabrik.de/asia/{area}-updates/\n"
    "https://download.geofabrik.de/australia-oceania/{area}-updates/\n"
    "https://download.geofabrik.de/central-america/{area}-updates/\n"
    "https://download.geofabrik.de/europe/{area}-updates/\n"
    "https://download.geofabrik.de/north-america/us/{area}-updates/\n"
    "https://download.geofabrik.de/south-america/{area}-updates/\n\n"

    "https://osm-internal.download.geofabrik.de/africa/{area}-updates/\n"
    "https://osm-internal.download.geofabrik.de/antarctica-updates/\n"
    "https://osm-internal.download.geofabrik.de/asia/{area}-updates/\n"
    "https://osm-internal.download.geofabrik.de/australia-oceania/{area}-updates/\n"
    "https://osm-internal.download.geofabrik.de/central-america/{area}-updates/\n"
    "https://osm-internal.download.geofabrik.de/europe/{area}-updates/\n"
    "https://osm-internal.download.geofabrik.de/north-america/us/{area}-updates/\n"
    "https://osm-internal.download.geofabrik.de/south-america/{area}-updates/\n\n"

    "https://planet.osm.org/replication/minute/\n"
    "https://planet.osm.org/replication/hour/\n"
    "https://planet.osm.org/replication/day/\n\n"

    "https://planet.openstreetmap.org/replication/minute/\n"
    "https://planet.openstreetmap.org/replication/hour/\n"
    "https://planet.openstreetmap.org/replication/day/\n\n";

  char   *usageDir =

    "DIR for --directory:\n"

    "This is the root or parent directory for the program working directory; where\n"
    "program files are kept. Program creates its own working directory with the name\n"
    "'getdiff', by default under current user {HOME} directory. With this option\n"
    "user may change this default to different directory which MUST exist with read,\n"
    "write and execute permissions / ownership permissions.\n"
    "Path can NOT end with 'getdiff' entry.\n\n";

  char   *usageBegin =

    "NUM for --begin:\n"

    "This argument is needed the first time you use 'getdiff' and ignored afterward.\n\n"
    "NUM is the full SEQUENCE NUMBER for change file to start downloading from.\n"
    "Sequence number is listed in that change file corresponding 'state.txt' file.\n"
    "The 'state.txt' file has timestamp and sequenceNumber lines; timestamp indicates\n"
    "date and time for latest data included in that change file. The sequenceNumber is\n"
    "a unique number for the change file, it is [4 - 9] digits long number.\n\n";

  char   *usageEnd =

      "NUM for --end:\n"

      "This argument is the sequence number for change file to end or stop download at.\n"
      "Together with 'begin' set this provides a RANGE of change files to download.\n"
      "With 'end' equals 'begin'; that change file and its corresponding state.txt are downloaded.\n"
      "Note that both sequence numbers must be from the same GRANULARITY (minute, hour or day).\n"
      "Downloaded file list are appended to 'rangeList.txt' file in working directory.\n\n";

  char   *usageName =

    "NAME for --user:\n"

    "Geofabrik internal server usage is restricted to account holders at openstreetmap.org.\n"
    "NAME is the user name for that account, you can use the email address used when\n"
    "you created your account with openstreetmap.org.\n\n"

	"SECRET for --passwd:\n"
    "SECRET is the password for the account mentioned above.\n"
    "The string length for both NAME and SECTRET can not be longer than 64 characters.\n"
    "Those arguments are required only for geofabrik.de internal server usage.\n\n";

  char   *usageConf =

    "FILE for --conf:\n"

    "FILE is user specified configuration file, must be in a directory with read and\n"
    "write permissions. Program will terminate if specified file is not accessible\n"
    "or empty - may have no setting at all; it can NOT have zero length bytes. See\n"
    "'Configuration File' section.\n\n";

  char   *usageNew =

    "ACTION for --new:\n"

    "By default each time program downloads new files, it appends the newly downloaded\n"
    "file names to file 'newerFiles.txt' in its working directory. To stop this behaviour\n"
    "use this option (No argument is required on the command line).\n\n";

  char   *limitations =

    "Limitations:\n"
    "\"getdiff\" limits the number of downloaded change files to avoid overwhelming the\n"
    "server. By default, it fetches at most 61 pairs of files in one run or session.\n\n";

  char   *confDscrp =

    "Configuration File:\n\n"

    "Configuration file is a text file with each line providing a directive pair of\n"
    "KEY and VALUE separated by space and an optional equal sign '='.\n"
    "Comment lines start with '#' or ';' characters, no comment is allowed on a\n"
    "directive line. KEYS correspond to command line options. Allowed directive KEYS set:\n"
    "{VERBOSE, USER, PASSWD, DIRECTORY, BEGIN, END, NEWER_FILE}.\n"
    "Unrecognized and duplicate directive 'KEYS' trigger error. You may set values to any\n"
    "number of KEYS or none.\n"
    "Default configuration file is {HOME}/getdiff.conf - user home directory. File is not\n"
    "required, not even default; but if specified then it must exist, be accessible and it\n"
    "can not be empty with zero byte length or size.\n\n"
    "Keep in mind set command line option overrides corresponding configuration value.\n\n"

    " VERBOSE : accepted values [on, true, 1] case ignored.\n\n"

    " SOURCE : same as --source option.\n\n"

    " DIRECTORY : same as --directory option.\n\n"

    " BEGIN : same as --begin option.\n\n"

    " END : same as --end option.\n\n"

    " USER : same as --user option.\n\n"

    " PASSWD : same as --passwd option.\n\n"

    " NEWER_FILE : same as --new option.\n\n";

  char *confExample =

    "The following is an example configuration file:\n\n"

    "# This is getdiff.conf file example; configuration file for getdiff program.\n"
    "# This line is a comment line (starts with #).\n"
    "; So is this one too (starts with ;). No comment is allowed on directive line.\n\n"

    "# To turn verbose mode on use: TRUE, ON or 1. Case ignored for TRUE and ON.\n"
    "# VERBOSE = True\n\n"

    "# USER : user name for openstreetmap.org account or email.\n"
    "# Required for internal server only, name is limited to 64 characters length.\n"
    "USER = johndoe\n\n"

    "# PASSWD: password for above account, also limited to 64 characters length.\n"
    "# PASSWD = mysecret\n\n"

    "# Try to always use \"https:\" NOT  \"http:\" even with public servers.\n"
    "# URL full path to region latest 'state.txt' file including protocol and server name\n"
    "# ending with your {region}-updates entry.\n"
    "SOURCE = https://osm-internal.download.geofabrik.de/north-america/us/arizona-updates/\n\n"

    "# DIRECTORY : the root directory for your 'getdiff' work directory.\n"
    "# path can NOT end with 'getdiff' entry.\n"
    "# User must have full permissions (read, write and execute) on this directory.\n"

    "DIRECTORY = /path/to/your/root/\n\n"

    "# BEGIN: sequence number for change file to start downloading from; [4 - 9] digits\n"
    "# number. Sequence number is listed in corresponding 'state.txt' file.\n"
    "BEGIN = 3264\n\n"

    "# Program appends new downloaded file names to 'newerFiles.txt' file in its\n"
    "# working directory; to turn this off specify one of ['none', 'off'] - case\n"
    "# ignored.\n"
    "# NEWER_FILE = NONE\n\n"

    "# End configuration example.\n\n";

  char   *howItworks =

    "THIS IS STILL UNDER CONSTRUCTION!\n\n"

    "How It Works:\n\n"

    "The \"getdiff\" program works by fetching new change files and their corresponding\n"
    "'state.txt' files from the specified server. The 'source' URL points to the region\n"
    "updates page on the server where the latest 'state.txt' file is found. The 'begin'\n"
    "argument is required the first time the program is used and ignored afterward; it\n"
    "specifies the sequence number for the change file to start the download from.\n\n"

    "If using geofabrik.de INTERNAL server, the program requires an OSM account user name\n"
    "and password. The user name and password are limited to 64 characters length, email\n"
    "used during account registration can be used as the user name. User name and password\n"
    "are sent to server in plain text format; they are never stored by the program.\n\n"

    "When executed program creates a working directory named \"getdiff\" for its own files.\n"
	"The working directory is created under current user {HOME} by default or as specified\n"
	"by the 'directory' option.\n"

    "Directories for supported servers are created in the working directory with 'geofabrik'\n"
    "and 'planet' entry names. Three directories are created under 'planet' with names: minute,\n"
    "hour and day entries. This is shown below:\n\n"

		  "  rootWD\n"
		  "     |--getdiff\n"
		  "          |--tmp\n"
		  "          |--geofabrik\n"
		  "          |--planet\n"
		  "               |--minute\n"
		  "               |--hour\n"
		  "               |--day\n\n"

    "Open street maps directory structure is mirrored under those directories above - entries are\n"
    "created as needed based on the path part of the downloaded file. Downloaded files are saved\n"
    "to the same path on the local machine as that on the remote server. \n"

    STYLE_BOLD
    "Please note that \"diff\" directory is no longer created or used.\n\n"
    STYLE_NO_BOLD


    "The 'newerFiles.txt' file in the working directory acts as a \"bucket\" storing a list\n"
    "of downloaded file names to destination directory. Downloaded file names are\n"
    "appended to this file. Another process is expected to use listed file names in this\n"
    "file to apply the update and remove 'newerFiles.txt' file when done; effectively\n"
    "emptying the \"bucket\".\n\n"

    "When using the 'range' function, program appends a list of downloaded files to file\n"
    "'rangeList.txt' in its working directory.\n\n"

    "Program writes minimal progress messages to terminal and log file in working directory,"
    "using 'verbose' option will generate extra messages for debugging.\n\n"


    "Program writes the following files in its working directory:\n\n"

    " - geofabrikCookie.txt : most recent geofabrik internal server cookie file - do NOT delete\n"
    " - getdiff.log: program log file, you need to empty it once in awhile.\n"
    " - newerFiles.txt: our \"bucket\" file, your script need to empty it.\n"
    " - previous.seq: sequence number of last downloaded change file.\n"
	" - latest.state.txt: state file for latest successfully downloaded file.\n"
    " - rangeList.txt: list of downloaded files when using 'range' function.\n\n"

	"Temporary files are written to 'tmp' directory under the program directory.\n\n"

    STYLE_BOLD
    "IMPORTANT: Do Not Delete 'previous.seq' from working directory.\n\n"
    STYLE_NO_BOLD
    "Typically one would store SOURCE, USER and DIRECTORY in a configuration file and\n"
    "start the program with :\n"
    " ~$ getdiff -c /path/to/my/getdiff.conf -p xxxxxxxx\n"
    " where xxxxxxxx is USER password for openstreetmap.org account.\n\n";

  char   *noticeTemplate =

    "                        Please NOTE the following:\n"
    "Geofabrik GmbH generously provides many free services to OSM communities, this\n"
    "deserves our sincere and deep gratitude. Please do not overuse this program or\n"
    "abuse geofabrik.de servers. Please do not download too many files in a short\n"
    "period of time. Please familiarize yourself with geofabrik GmbH by visiting\n"
    "their web site : www.geofabrik.de.\n"
    "Thank you all in advance for following this request.\n\n";

  fprintf(stdout, description, progName);

  fprintf(stdout, usage1, progName);

  fprintf(stdout, usageURL);

  fprintf(stdout, usageDir);

  fprintf(stdout, usageBegin);

  fprintf(stdout, usageEnd);

  fprintf(stdout, usageName);

  fprintf(stdout, usageConf);

  fprintf(stdout, usageNew);

  fprintf(stdout, limitations);

  fprintf(stdout, confDscrp);

  fprintf(stdout, confExample);

  fprintf(stdout, howItworks);

  fprintf(stdout, "%s%s%s", STYLE_BOLD, noticeTemplate, STYLE_NO_BOLD);

  exit (ztSuccess);
}

void shortHelp(void){

  /* this is needed when source argument is missing - not used?! **/
  fprintf(stdout, " Try: %s --help\n\n", progName);

  exit(0);

}

