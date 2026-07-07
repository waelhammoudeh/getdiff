# Changes:
  - Version: 0.01.90 Date July 7/2026
    - rename "changeFiles.md to README.changeFiles.md
    - updated usage.c makefile getdiff.h

  - Version: 0.01.89 Date: July 2/2026
    - Bug fix: allow single change file download.

  - Version: 0.01.87 Date: June 26/2026
    - getdiff: added `--log` option & 'LOG_FILE' key in configure file, user may set log file
      to something other than default: {workDir}/getdiff.log
    - makefile: added `install` target, program can now be installed with `make install`
    - SlackBuild: added Slackware build script
    - Documnetation redone to be more clear - I hope!

  - Current Version: 0.01.81 Date: June/11/2025
    - Many memory bug fixes
  - Version: 0.01.77
  - New 'end' option: Specifying 'begin' and 'end' arguments provide a new function to download a RANGE of files between specified sequence numbers.
  - Implemented a lock file mechanism to limit work directory access to one instance of the program.
  - Fully supports OSM planet servers.
  - Removed "diff" directory from working directory. New downloads destination is based on server name and file path.
    **NOTE:** My "op_update_db.sh" script has been updated to use the new path, use updated script.
  - The downlod2File() in curl functions got a progress meter.
  - Code to retrieve cookie (using Geofabrik python script) handles no output from the latest script. New script does not provide any output of any kind!
  - Improved and restructured code, hopefully it is easier to understand and follow.

# Getdiff

Program to download Open Street Maps differ files  also known as "OSM change files" from remote server.

Program uses change files SequenceNumber to accomplish its work. See [README.changeFile.md](README.changeFiles.md)
for a condensed summary of what you need to know about OSM change files. The program
is written in C programming language, uses curl library to achieve its goal.

This command line program aims to ease the process of updating OSM data files and databases that
use OSM change files. The basic idea is to break the update process into two steps; collecting
differ (change) files first, and second applying collected differ files to do the update. Program
`getdiff` performs the first step of collecting differ files by downloading them from a specified
remote server to local machine. Another program or script uses those downloaded files to apply
the update to database or OSM data file. Program downloads differ files and their corresponding
state.txt files from remote server. Program appends downloaded filenames (and their local path)
to `newerFiles.txt` file in its working directory - which is intended to be used by the updater script
or program. The updater is expected to remove this `newerFiles.txt` file once done processing to
avoid its growing size.

Program does NOT do the update itself.

Supported servers are:
  - https://download.geofabrik.de/
  - https://osm-internal.download.geofabrik.de/
  - https://planet.osm.org/
  - https://planet.openstreetmap.org/

When accessing Geofabrik Internal Server a cookie is required; the program executes "oauth_cookie_client.py"
script from [Geofabrik](https://github.com/geofabrik/sendfile_osm_oauth_protector/blob/master/doc/client.md)
as is (no modification) with user's credentials for [openstreetmap.org](https://openstreetmap.org) account
to retrieve a cookie. Program maintains the cookie which is used in all curl-library communications with
Geofabrik Internal Server. Program assumes python3 interrupter (executable) to be found in the default
slackware installation; that is '/usr/bin/python3'.


## Building:

To compile the program, ensure you have "libcurl" version 7.80.0 or later installed on your system.
A makefile is provided; run `make` and `make install` to compile and install on your Linux system.

A Slackware build script is also provided; to build Slackware package place the source tarball in
the SlackBuild directory then build and install the package. Note: URL to source tarball is in the
info file.

## Usage:

Program accepts input from the command line and a configuration file, or a combination of the two.

On startup program creates its working directory where it stores its own files and where it stores
downloaded differ files and their corresponding state.txt files. Directories are created as shown
below:

```
getdiff
  ├── tmp
  ├── geofabrik
  └── planet
      ├── day
      ├── hour
      └── minute
```

By default working directory is created under current user {HOME} directory, this can be changed by
using `--directory` command line option or 'DIRECTORY' configuration file key - use full path for directory
in configuration file.

Working directory has two restrictions; first it is always called "getdiff", second this entry name can not
be nested - can not use {SOME_PATH}/getdiff/getdiff.


Configuration file is optional, by default program looks for it under current user {HOME} with name
"getdiff.conf", that is: {HOME}/getdiff.conf. User may change that with command option `--conf`, if that
is used then specified file must exist and be non-empty file.

Program writes its progress to its logging file by default in its working directory with name "getdiff.log"
that is: getdiff/getdiff.log. User can change that with `--log` option or 'LOG_FILE' configuration key.

To use the program provide the URL to remote server as `--source` argument or use 'SOURCE' key in configuration
file. This is the URL where remote server lists latest available change file.

On first time use of this program, provide the sequence number for change file to start downloading from
as `--begin` option argument or value for 'BEGIN' configuration key.

If you use Geofabrik INTERNAL server, then you must provide user name and password for "openstreetmap.org"
account - required to access INTERNAL server at "geofabrik.de" site. You may use the email used to create
the account as the user name here.

Files downloaded from "Geofabrik.de" are saved under "geofabrik" directory and those from "plane.osm.org" under
"planet" directory under program working directory - see tree structure above.

Program writes the sequence number for latest downloaded change file in "previous.seq" file in its working
directory. The "previous.seq" file is checked for every time the program is used, its absence indicates
program first time use - you should not remove this file.

Program appends the names of downloaded files to `newerFiles.txt` file in its working directory. The path
included in the names is the path part from getdiff download directory, prepend that to get file full path.

Program does not maintain or check `newerFiles.txt`, it is expected to be maintained by an updater script
or program. You may stop appending to the file with `--new` command line switch or use 'NEWER_FILE' key with
value set to none or off.

**Range Function:**

By specifying and setting `--begin` and `--end` options on the same invocation, program will download
change files and their corresponding state.txt files in the range of the specified begin and end sequence
numbers inclusive.
Specified sequence numbers must be from the same Granularity; that is both are for minute, hour or day
change files. Program appends a sorted list of downloaded files to `rangeList.txt` file in its working
directory each time it is invoked with the range function. You may mix lists in `rangeList.txt` file if
desired, doing so enables you to cover any arbitrary time period.

See [here](https://github.com/waelhammoudeh/overpass-4-slackware/tree/master/Extract_and_Planet_Change_Files)
for real example of using the program range function.


Wael Hammoudeh

July 2/2026
