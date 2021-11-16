#!/bin/sh
set -e

# Options
RPATH_NAME=$1
EXEC=$2

# Function to keep only the second arg
get_second_arg() {
    echo $2
}

# Get current rpath
WANTED_RPATH=$(get_second_arg $(otool -l $EXEC | grep $RPATH_NAME | grep path))

if [ ! -z "$WANTED_RPATH" ]; then
    install_name_tool -delete_rpath $WANTED_RPATH $EXEC
fi