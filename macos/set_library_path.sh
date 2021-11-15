#!/bin/sh
set -e

WANTED_LIB=$1
NEW_PATH=$2
EXEC=$3

get_first_arg() {
    echo $1
}

CURRENT_RPATH=$(get_first_arg $(otool -L $EXEC | grep $WANTED_LIB))

echo $CURRENT_RPATH