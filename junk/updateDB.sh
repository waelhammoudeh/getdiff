#!/bin/bash

# this is STILL junk shit

INFILE=$1
VERSION=$2

EXEC_DIR=/usr/local/bin
DISPATCHER=$EXEC_DIR/dispatcher
UPDATE_EXEC=$EXEC_DIR/update_database
OSMIUM=$EXEC_DIR/osmium
DB_DIR=/mnt/nvme4/op-az-latest-220408/
DB_DIR=/mnt/nvme4/op-attic
# DB_DIR=$($DISPATCHER --show-dir)

# META=--meta
META=--keep-attic
COMPRESSION=gz
FLUSH_SIZE=16

echo "DB_DIR is $DB_DIR"

set -e

if [[ -z $2 ]]; then
    echo "$0: Error missing argument(s); 2 are required"
    echo "usage: $0 INFILE VERSION"
    echo "        where"
    echo "INFILE is OSM input file: any osmium supported format"
    echo "VERSION is version number to use - last date in INFILE"
    exit 1
fi

$OSMIUM cat $INFILE -o - -f .osc | $UPDATE_EXEC --db-dir=$DB_DIR \
                                                             --version=$VERSION $META \
                                                             --flush-size=$FLUSH_SIZE \
                                                             --compression-method=$COMPRESSION

# initialed op-attic with this file by using the following command line:
# time nohup ./updateDB.sh arizona-internal-2022-4-10.osh.pbf 2022-04-08
#
# Output below:
# overpass@yafa:/mnt/nvme4/op-attic/source$ time nohup ./updateDB.sh
# arizona-internal-2022-4-10.osh.pbf 2022-04-08 &
# real	60m57.776s
# user	54m55.470s
# sys	7m25.077s

# get date from timestamp line : timestamp=2021-11-18T21\:21\:39Z
#
# wael@yafa:~/Downloads/getdiff/diff$ cat 201.state.txt | grep timestamp | cut -d 'T' -f -1 | cut -d '=' -f 2
# output : 2021-12-31

# wael@yafa:~/Downloads/getdiff/diff$ cat 306.state.txt | grep timestamp | cut -d 'T' -f -1 | cut -d '=' -f 2
# 2022-04-15
# wael@yafa:~/Downloads/getdiff/diff$

