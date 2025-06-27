#!/bin/sh
#finder.sh

if [ -z $1 ]; then
    echo "must specify filepath arg"
    exit 1
fi



if [ -z $2 ]; then
    echo "must specify search string arg"
    exit 1
fi

FILESDIR=$1
SEARCHSTR=$2

PREFIX="/"
FILESDIR=${FILESDIR#"$PREFIX"}

if ! [ -d $FILESDIR ]; then
    echo "invalid filepath arg"
    exit 1
fi

FILE_COUNT=$(find $FILESDIR -type f | wc -l)
WORD_COUNT=$(grep -rn $SEARCHSTR $FILESDIR | wc -l)

echo "The number of files are $FILE_COUNT and the number of matching lines are $WORD_COUNT"
