# Updated
This is a complete rewrite of the program with major changes to its code and required
arguments.


# Getdiff
Program to fetch or download OpenStreetMap differ files for a specified region (area) from "www.geofabrik.de" web site public or internal servers.

Differ files on Geofabrik servers are daily diff updates (.osc.gz, Gzip compressed OSM XML). They contain the changes of one day. Regional extract files can be updated with them.

The program is a command line tool written using C programming language intended to ease updating regional data extracts, can be called from scripts. Program does not update any data.

Program uses "oauth_cookie_client.py" script from [Geofabrik github repository](https://github.com/geofabrik/sendfile_osm_oauth_protector/blob/master/doc/client.md). Please see [LICENSE.md](/LICENSE.md).
The script above is used to retrieve login cookie from OpenStreetMap.org when geofabrik internal server is used.
This program assumes python3 executable to be found in the default slackware installation; that is '/usr/bin/python3'.

## Building:
Program requires "libcurl" version 7.80.0 or later. A makefile is included in the root directory; to compile run 'make' in that directory.

## Improvements
  - Utilized change files sequence number to locate files on remote servers and on local machine.

  - C-Library time functions are used to compare dates.

  - Improved Curl Library function calls and error handling.

  - Refactored HTML parsing functions.

  - Implemented periodic time delay when downloading as not to overwhelm remote server.


# Changes:

The following command line arguments have changed as specified below:

### source:
  - URL argument is the internet address pointing at the region updates page at geofabrik.de server.

### begin:
  - NUM is the sequence number for change file to start download from.

### new
  - ACTION only accepted values for ACTION is one of ['none' or 'off'] case ignored.
  The effect is to stop generating / appending to 'newerFiles.txt' file in working directory.

### Entries in newerFiles.txt file:

Entries for new downloaded files are prepended by a partial path, appending an
entry line to the download "diff" directory path in your system results in the full
path in your machine for the new downloaded files.
An example of the new format is below:

```
  /005/637/617.osc.gz
  /005/637/617.state.txt
  /005/637/618.osc.gz
  /005/637/618.state.txt
  /005/637/619.osc.gz
  /005/637/619.state.txt
```


## Usage:
Program accepts input from the command line, configuration file or combination of both.
Command line option overrides the same configuration settings. Default configuration file is used unless changed by user.
Configuration file is NOT required, a warning is issued if no configuration file is set and defaults were not found.
```
Usage: getdiff [OPTION]

   -h, --help          Show this information page.
   -V, --version       Prints program version and exits.
   -v, --verbose       Prints progress messages to terminal. Default is off.
   -s, --source URL    Set internet address URL to point to your region updates directory at
                       geofabrik.de server; where the latest 'state.txt' file is found.
   -d, --directory DIR Set the root directory for the program working directory; default is
                       user {HOME} directory.
   -b, --begin NUM     Specify sequence number for the change file to start downloading from.
                       This argument is required for first time use and ignored afterward.
   -u, --user NAME     Specify OSM account user name. Required for geofabrik internal server.
   -p, --passwd SECRET Specify the password for the account above.
   -c, --conf FILE     Configuration file to use, default {HOME}/getdiff.conf
   -n, --new ACTION    Specify the action for newly downloaded files. By default, the program
                       appends the newly downloaded file names to 'newerFiles.txt'. Use 'None'
                       or 'Off' to disable this behavior.
Configuration file:

Configuration file is a text file with each line providing a directive pair of
KEY and VALUE separated by space and an optional equal sign '='.
Comment lines start with '#' or ';' characters, no comment is allowed on a
directive line. KEYS correspond to command line options. Allowed directive KEYS are:
{VERBOSE, USER, PASSWD, DIRECTORY, BEGIN, NEWER_FILE}.
Unrecognized and duplicate directive 'KEYS' trigger error. You may set values to any
number of KEYS or none.
Keep in mind set command line option overrides configuration value.

 VERBOSE : accepted values [on, true, 1] case ignored.

 SOURCE : same as --source option.

 DIRECTORY : same as --directory option.

 BEGIN : same as --begin option.

 USER : same as --user option.

 PASSWD : same as --passwd option.

 NEWER_FILE : same as --new option.

```

#### Option Arguments:
```
URL for --source:
This is the full URL to the region updates directory at geofabrik.de server.
This starts with protocol followed by server name followed by continent and maybe
country or area name at geofabrik.de server, ending with entry {area}-updates.
where {area} is your area or country name.
In all cases this path is where the latest 'state.txt' file is found.

DIR for --directory:
This is the root or parent directory for the program working directory; where
program files are kept. Program creates its own working directory with the name
'getdiff', by default under current user HOME directory. With this option
user may change this default to different directory which MUST exist with read,
write and execute permissions / ownership permissions.
Path can NOT end with 'getdiff' entry.

NUM for --begin:
This argument is needed the first time you use 'getdiff' and ignored afterward.

NUM is the full SEQUENCE NUMBER for change file to start downloading from.
Sequence number is listed in that change file corresponding 'state.txt' file.
The 'state.txt' file has timestamp and sequenceNumber lines; timestamp indicates
date and time for latest data included in the change file. The sequenceNumber is
a unique number for the change file, it is [4 - 9] digits long number.
Change and its corresponding 'state.txt' file names are the THREE least significant
digits in the sequenceNumber. Know the last date for data included in your region
data file you want to update, then examine state.txt files for date just after
that one. Provide full sequenceNumber from that 'state.txt'.

NAME for --user:
SECRET for --passwd:
Geofabrik internal server usage is restricted to account holders at openstreetmap.org.
NAME is the user name for that account, you can use the email address used when
you created your account with openstreetmap.org.
SECRET is the password for that account.
The string length for both NAME and SECTRET can not be longer than 64 characters.
Those arguments are required only for geofabrik.de internal server usage.

FILE for --conf:
FILE is full path and file name for user specified configuration file, must be in
a directory with read and write permissions. Program will terminate if specified
file is not accessible or empty.

ACTION for --new:
By default each time program downloads new files, it appends the newly downloaded
file names to file 'newerFiles.txt' in its working directory. To stop this behaviour
use one of 'None' or 'Off' for action.

```
#### Setting --begin NUM argument:
```
NUM is the complete sequence number for change file to start download from.

Each change file has a corresponding 'state.txt', the 'state.txt' file has two
key information; the first is the sequence number and the second is the date
for last included data in the change file.

The sequence number is 4 to 9 digit unique number for each change file, it is used
to indetify the change file and also to locate change file within the file system.

Data in OSM data files are dated, when data is added or modified it gets time stamped.

Your area or region data file has a date for last included data in it, on geofabrik.de
download page for your region this date is stated in line on top of page; example:

"last modified 1 day ago and contains all OSM data up to 2022-04-09T20:21:55Z. File size: 195 MB;"

This date can also be retrieved from the OSM data file with "osmium fileinfo"
using the "--extended" option as:
 osmium fileinfo --extended regionfilename.osm.pbf

note that regionfilename.osm.pbf can be any supported file type by osmium.

To find out which file to start download from, we browse through 'state.txt' list.
Look for date just AFTER that date of your region data file you want to update.
The sequence number from the matched 'state.txt' file is the number needed.

Keep in mind that geofabrik.de keeps differ files for a period of three to four months.
So if your data is older, you need more recent OSM data file for your region.

```
#### Typical Usage:

Typically one would store SOURCE, USER and DIRECTORY in a configuration file and
start the program with :
```
 ~$ getdiff -c /path/to/my/getdiff.conf -p xxxxxxxx
 where xxxxxxxx is USER password for openstreetmap.org account.

```

For an updating strategy; automation and implementation, check out my repository for [overpass-4-slackware](https://github.com/waelhammoudeh/overpass-4-slackware)
where "getdiff" is used along with bash scripts to automatically update an initialed and live overpass
database.

This program can be used with OSM planet servers; **not fully tested here**; try one of following URLs:
```
https://planet.osm.org/replication/minute/
https://planet.osm.org/replication/hour/
https://planet.osm.org/replication/day/

https://planet.openstreetmap.org/replication/minute/
https://planet.openstreetmap.org/replication/hour/
https://planet.openstreetmap.org/replication/day/
```

#### Limitations:
"getdiff" limits the number of downloaded change files to avoid overwhelming the
server. By default, it fetches at most 61 pairs of files; (change and state.txt)
in one run or session.

### I need your feedback please:

If you use this program and have any comment or question, please feel free to email me at: w_hammoudeh@hotmail.com

Enjoy

Wael Hammoudeh
