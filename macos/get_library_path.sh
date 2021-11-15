#!/bin/sh
set -e

# Options
WANTED_LIB=$1
EXEC=$2

# Function to extract the first element of a space seperated list
get_first_arg() {
    echo $1
}

# Get current path
echo $(get_first_arg $(otool -L $EXEC | grep $WANTED_LIB))