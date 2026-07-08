This file presents few definitions and breifly discuss OSM change files. Terms
and ideas presented here should be famaliar to anybody interested in OSM data files.
The use of a web browser is helpful to locate files on remote servers.

**Change Files:** Change file is the difference between two OSM data files from two
different points in time. Merging the change file with the older data file results
in the newer data file.

When a new change file is calculated; it is assigned a **timestamp** and a unique
**replication sequence number** (sequence number for short) both are written to a
corresponding "state.txt" file. Given one variable, to retrieve the other variable
we look in the "state.txt" file for that.

Change files are usually named with numbers, corresponding state.txt file will start
with the same numbers as change file. See below for naming scheme.

Planet change files are calculated for three different time intervals (called Granularity),
those are minutely, hourly and daily change files calculated at the top of each time period.

Change files from Geofabrik.de servers are daily change files calculated between
20:00 and 21:00 UTC for most regions. Change files from Geofabrik.de INTERNAl and
PUBLIC server are calculated at the same time each day so they share the same
naming numbers and the same "state.txt" files.

**Timestamp and Change file:** We know that each element in OSM data file has its own
timestamp, the **change file timestamp** indicates that it includes elements with
**up to that specified timestamp.** Change files cover a defined period of time; defined
means it has **start** and **end** time. The **change file timestamp** is always the
**end time**. The implied **start time** in **minutely** change file is **one minute before**
its end time, for an **hourly** change file its start time is **one hour before** its end time,
and for a daily change file the implied start time is **one day before** its end time
indicated by its timestamp.

**Sequence Number and Change File:** Formally called "replication sequence number" is
a unique number assigned to change file when calculated **by replication service provider.**
For planet change files; sequence numbers are different for each Granularity, which
must be given with each sequence number. Geofabrik sequence numbers are for daily
change files from Geofabrik only. Sequence number is an integer of 9 digits or less.

Timestamps and sequence numbers can be used to locate change file on replication servers.

**Locate Change File By Timestamp:** Change files are placed into directory hierarchy
on replication service provider sites (like planet.osm.org/replication/day). Starting
from the top directory we traverse file system hierarchy using the "Last modified"
time for directory entries and our timestamp (consider directory "Last modified" time
as a store closing time). One way to do that is locate the closest "Last modified"
time to our timestamp, if this closest "Last modified" time is after our timestamp -
store is still open - then we look in that directory - we made it before its closing
time. If "Last modified" time is before our timestamp - we missed that directory
closing time - we look in the next directory up to that with closest "Latest modified"
time. Applying the same principle until we find a directory we can enter.

Usually timestamp you see in OSM files are formated slightly different than the
"Last modified" time for file system. You may reformat your timestamp to match
that "Last modified" time. Replace the "T" with "space" separating the "date" part
from the "clock" part, remove "back slashes or slashes" if present and finally drop
the "seconds" part from the "clock" part:
```
2007-08-10T17:38:36Z ---> 2007-08-10 17:38

2026-07-07T20\:21\:26Z ---> 2026-07-07 20:21
```
The last format is what you see for "Last modified" time on servers.

**Locate Change File By Sequence Number:** Sequence numbers are used to store files
on disks. The following scheme is used to store change files on disks by converting
sequence number to file system structure:

Sequence number is placed into a nine-digit long field padded with leading zeros -
when needed, the string is then split into 3 fields 3 character long each, three
slashes are inserted starting from the left:
Note: column heading "# 0s" is for number of digits present in the sequence number,
then the number of zeros required for padding.

 <pre>
    sequence #    # 0s  123456789      separate 3x3  insert slashes

     6756477 ---> (7+2) 006756477 ---> 006 756 477   /006/756/477
      113123 ---> (6+3) 000113123 ---> 000 113 123   /000/113/123
        4537 ---> (4+5) 000004537 ---> 000 004 537   /000/004/537
</pre>

last we append extensions (.osc.gz & .state.txt) to form change file and its corresponding
state.txt file names:

<pre>
     string               change file           state.txt file

   /006/756/477 ---> /006/756/477.osc.gz and /006/756/477.state.txt
   /000/113/123 ---> /000/113/123.osc.gz and /000/113/123.state.txt
   /000/004/537 ---> /000/004/537.osc.gz and /000/004/537.state.txt
</pre>

The path part above is appended to replication provider root page.

**Extract OSM Data File Info:** OSM data files contain meta information about the
file. We use **"osmium fileinfo"** to retrieve and view this information, using
the --extended (-e) option with the command gives more information.
Man osmium-fileinfo for its full usage.

My SlackBuild for osmium tool is [here.](https://github.com/waelhammoudeh/osmium-tool_slackbuild)

Using an example extract data file, selected and important lines from "osmium fileinfo -e "
are shown and explained below:

<pre>
File:
  Name: region/extract/arizona-internal.osm.pbf

Header:
    osmosis_replication_base_url=https://osm-internal.download.geofabrik.de/north-america/us/arizona-updates
    osmosis_replication_sequence_number=4535
    osmosis_replication_timestamp=2025-09-03T20:20:35Z

    timestamp=2025-09-03T20:20:35Z

Data:

  Timestamps:
    First: 2007-08-10T17:38:36Z
    Last: 2025-09-03T20:00:57Z

</pre>

 - Name: file name given in the command line

 - osmosis_replication_base_url: this is the URL for update page for your OSM extract data file.

 Copy this URL and paste it into your browser address bar; take yourself into a tour, note the
 listed "state.txt" file - for latest available change file - take a look inside this state.txt file.
 Then click on links to see how change files and their state.txt files are organized. Take the same
 tour after few days and see how often files are updated.

 Note that you need to login into your "openstreetmap.org" account to access Geofabrik.de INTERNAL
 server. You may want to create an account if you do not have one!

 - osmosis_replication_sequence_number: sequence number for **last included** change file,
 start update from **next** sequence number; more about this below.

 - osmosis_replication_timestamp: **(Header)** extract calculation time; extract includes
 data **up to this timestamp** use this timestamp as starting point to bring your
 extract OSM data file in alignment with planet daily update.

 - Timestamps: The *Data* section lists included data timestamps for First and Last
 data elements among other information about the input file.

The Geofabrik download page for extract includes the timestamp for that extract in
the same line it states the extract creation time; something like:

<pre>
This file was last modified 1 day ago and contains all OSM data up to 2025-09-30T20:21:27Z.
</pre>

The information above is set in recent extract files from Geofabrik, it may not
all be set from other extract sources. The **data timestamps** should always be
set in (.osm.pbf) files.

### Geofabrik "state.txt" File Has Planet Minutely Sequence Number:

Back to the sequence number above; use "Locate change file by sequence number" method
described above to locate and view state.txt file for Geofabrik sequence number = 4535:

```
4535 ---> 000004535 ---> /000/005/435.state.txt
```

Locating state.txt file above on Geofabrik server (under update page URL), we find the
file below:

<pre>
 # original OSM minutely replication sequence number 6756480
 timestamp=2025-09-03T20\:20\:35Z
 sequenceNumber=4535
</pre>

Notice the timestamp value matches timestamp from osmium fileinfo output.

The first commented out line is the important part in this file; the sequence number
listed is the last minutely change file included in this extract file from **planet server.**
We locate file with sequence number 6756480 at planet minute server:

```
6756480 ---> 006756480 ---> /006/756/480.state.txt
```

We locate file on planet minute server: "https://planet.osm.org/replication/minute",
this file contents are:

<pre>
#Wed Sep 03 20:21:06 UTC 2025
sequenceNumber=6756480
timestamp=2025-09-03T20\:20\:35Z
</pre>

To update this data file using minutely change files from planet OSM server we start
at minute sequence number **just after** 6756480.

The same state.txt on planet minute server can be found using the timestamp value
listed from osmium fileinfo using "Locate Change File By Timestamp" method described
above.
