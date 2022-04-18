#!/bin/bash

# draft script to update my overpass database

# empty array
declare -a newer_lines=()
input=/home/wael/newer-files.txt

#if [ -f newer-files.txt]; then check for file
# read ONE into array member
while IFS= read -r line
do
{
    newer_lines+=($line)
}
done < "$input"

for new_line in "${newer_lines[@]}"
    do
        echo "$new_line"
    done

length=${#newer_lines[@]}

echo "length is: $length"

# group per pair change & state
i=0
j=0

while [ $i -lt $length ]
do
    j=$((i+1))
    changeFile=${newer_lines[$i]}
    stateFile=${newer_lines[$j]}

    echo "i is <$i> changeFile is $changeFile"
    echo "J is <$j> stateFile is $stateFile"

# get number from file name
    changeFileNumber=`echo "$changeFile" | cut -f 1 -d '.'`
    echo "changeFileNumber is: $changeFileNumber"

   stateFileNumber=`echo "$stateFile" | cut -f 1 -d '.'`
    echo "stateFileNumber is: $stateFileNumber"

# they better be the same numbers
    if [[ $changeFileNumber -ne $stateFileNumber ]]; then
        echo "Not same files; change and state!"
        exit 1
    fi

# get date only YYYY-MM-DD & use as version number
    VERSION=`cat $stateFile | grep timestamp | cut -d 'T' -f -1 | cut -d '=' -f 2`

    echo "VERSION is $VERSION"

# move to next pair
     i=$((j+1))
    echo
done
