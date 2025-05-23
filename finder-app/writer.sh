#!/bin/bash
#writer.sh

if [ -z $1 ]; then
    echo "must specify file arg"
    exit 1
fi

if [ -z $2 ]; then
    echo "must specify write string arg"
    exit 1
fi

WRITEFILE=$1
WRITESTR=$2

PREFIX="/"
WRITEFILE=${WRITEFILE#"$PREFIX"}

if [ -f $WRITEFILE ]; then
    rm -rf $WRITEFILE
fi

mkdir -p "$(dirname $WRITEFILE)" && touch "$WRITEFILE"

if [ -f $WRITEFILE ]; then
    echo "$WRITEFILE created"
else
    echo "filed to create $WRITEFILE"
fi

echo "$WRITESTR" > $WRITEFILE
