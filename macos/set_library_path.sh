#!/bin/sh
set -e

# Options
WANTED_LIB=$1
NEW_PATH=$2
EXEC=$3

# Function to extract the first element of a space seperated list
get_first_arg() {
    echo $1
}

# Get current path
CURRENT_PATH=$(get_first_arg $(otool -L $EXEC | grep $WANTED_LIB))

# Change to the new path
install_name_tool -change $CURRENT_PATH $NEW_PATH $EXEC