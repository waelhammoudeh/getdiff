# Getdiff
Program to fetch or download OpenStreetMap differ files for a specified region (area) from "www.geofabrik.de" web site public or internal servers.

Differ files on Geofabrik servers are daily diff updates (.osc.gz, Gzip compressed OSM XML). They contain the changes of one day. Regional extract files can be updated with them.

The program is a command line tool written using C programming language intended to ease updating regional data extracts, can be called from scripts. Program does not update any data.

Program uses "oauth_cookie_client.py" script from [Geofabrik github repository](https://github.com/geofabrik/sendfile_osm_oauth_protector/blob/master/doc/client.md). Please see [LICENSE.md](/LICENSE.md).

## Building:
Program requires "libcurl" version 7.80.0 or later. A makefile is included in the root directory; to compile run 'make' in that directory.

## Usage:
Program accepts input from the command line, configuration file or combination of both. Command line option overrides the same configuration settings. Default configuration file is used unless changed by user.
Configuration file is NOT required, a warning is issued if no configuration file is set and defaults were not found.
```
Usage: %s [ option optarg ]
   -h, --help              Show help information page.
   -v, --verbose           Prints progress messages.
   -s, --source URL        Specify full URL with area differ files list.
   -b, --begin NUM         Specify 3 digits differ file number to start download from.
   -u, --user NAME         Specify OSM account user name.
   -p, --passwd SECRET     Specify OSM account password.
   -d, --directory DIR     Program work directory, default {USERHOME}/Downloads/getdiff/.
   -c, --conf FILE         Configuration file to use, default /etc/getdiff.conf
   -n, --new FILE          File (path + name) append list of downloaded file to.

Configuration file is a text file with each line providing a directive pair of NAME and VALUE
separated by space or tab and an optional equal sign '='.
Comment lines start with '#' or ';' characters, no comment is allowed on a directive line.
Directive names in configuration file are all capitalized, not to be changed and similar to
command line options above. Unrecognized directive name triggers an error. Directive
names are as follows:

 VERBOSE   : accepted values {on, off, true, false, 1, 0} case ignored for value.
 SOURCE    : same as --source option above.
 DIRECTORY : same as --directory option above.
 BEGIN     : same as --begin option above.
 USER      : same as --user option above.
 PASSWD    : same as --passwd option above.
 NEWER_FILE : same as --new option above.
```

#### Option Arguments:
```
--source URL: full path to area differ files list including geofabrik server
              name. This is the URL to your area used in browser to view
              available differ files. It includes the area name and ends with
              numbered directory entries /000/003/.

--directory DIR: This is the program work directory, must end in "getdiff" name.
                 "getdiff" is added to argument "DIR" if missing, log file and
                 other program files are saved to work directory. Program creates
                 "diff" directory under this work directory where downloads are
                 saved in. Default is "{USERHOME}/Downloads/getdiff/" where
                 {USERHOME} is current user home directory.

--begin NUM: The three digit number from differ file name to start download from.
             For initial use of program only, after first use, program writes and
             uses "start_id" file in its work directory and ignores this option.
             Each differ file has a corresponding state file which we download also.
             Please see "Setting --begin NUM argument" below.

--user NAME: OpenStreetMap account user name, required to access the internal
             server at "https://osm-internal.download.geofabrik.de". Name is
             limited to 64 character length for this program. This option is
             ignored when geofabrik public server is in use.

--passwd SECRET: The password for the above account in plain text. This option
                 is ignored when geofabrik public server is in use.

--conf FILE: The user configuration file to use. Default is /etc/getdiff.conf.
             If that is not found, program looks in its own executable directory
             for "getdiff.conf" as a second default alternative.

--new FILE: File is path and name where just downloaded files list is appended to.
            List is to be used by "update script", script should remove file
            when done.
```
#### Setting --begin NUM argument:
```
NUM is the number part in the differ file name to start download from.

First thing to know is the last date included in your OSM data file to update.
The region download page on geofabrik.de site lists this date, for example:
"last modified 1 day ago and contains all OSM data up to 2022-04-09T20:21:55Z. File size: 195 MB;"
This date can also be retrieved from the OSM data file with "osmium fileinfo"
using the "--extended" option as:
 osmium fileinfo --extended regionfilename.osm.pbf

note that regionfilename.osm.pbf can be any supported file type by osmium.

Unlike OSM (.osc) change files, region files are not generated daily. This is another
reason to use a tool like osmium.

The second thing to know is that geofabrik.de keeps differ files for a period of
three months. So if your data is older, you need more recent OSM data file for
your region.

Finally, each differ file has a corresponding state file containing time stamp
and sequence number. This time stamp is the time the differ was generated. This
is usually - not always - very close to last date for data contained in (.osc)
differ file.

Examine the date the time stamp line has, set NUM to file name number where this
date is just before last date in your region OSM data file.
```
#### Typical Usage:

The configuration file is optional but it makes using the program a lot easier, use it to set most settings you use.
It also enables you to update multiple regions or extracts by having different configuration files. With most used
settings in configuration file - I do not set password - I start mine with "conf" and "passwd" options only:
```
   wael@yafa:~$ /mypath/getdiff -c ~/getdiff.conf -p *********
```
where I type my password for that option. You do not need to use all settings in configuration file, leave unused options
commented out with '#' character or ';' character. For me the "=" character makes reading and editing the file easier.

## How Does It Work:

Program is written using C. Demonstrates parsing command line, parsing html file, parsing cookie file.
String handling in C is shown. Uses libcurl, uses structures like double linked list. System calls are used. Forking processes in Linux.

### Program workflow:

This is work in progress ...

Lots of documentation still to add.

###### The short story:

OSM (.osc) differ files and their (.state.txt) files are listed on Geofabrik website as an HTML page, which has URL address;
this address is a required argument for this program. Using this address, program fetches
this page as the index.html file.
In the case of Geofabrik public server, the page URL is all that is required. However for
the Geofabrik internal server case; one needs to be logged into OpenStreetMap.org own
account so the user account name and password are also required to fetch this "index.html"
page. Program saves the source for this page in its work directory each time it is used.

This HTML is downloaded using "libcurl" functions. Program parses this HTML page using
standard C library functions then places extracted file names into a sorted string list.
Users do not - and should not - need to download this whole list for their updates, so
this program sets a starting file name from the sorted list then downloads files to the
end of the list. Note sorted list has both change and state files.

In the internal server case, a token string is required to access any file. Geofabrik provides
a python script ["oauth_cookie_client.py"](https://github.com/geofabrik/sendfile_osm_oauth_protector/blob/master/doc/client.md) to retrieve the cookie.
The cookie format and the script usage are documented on Geofabrik github repository.
This program calls this script  (by forking) with argument to save cookie as a text file, program parses this text file to get access token string and expiration date. Each time the internal server is used,
program checks expiration date and gets new cookie only if it has expired. The "access token string" is used in "libcurl" function calls.

Program writes the sorted list as a text file, logs its progress to a log file.
Program can append most recent downloaded file names to user set file name; user may use
this file as helper file when writing "update script".
