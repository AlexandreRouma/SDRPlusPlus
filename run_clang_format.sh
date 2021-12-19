#!/bin/bash

TOTAL_LINES=0
FILE_COUNT=0

echo Searching directories...
CODE_FILES=$(find . -iregex '.*\.\(h\|hpp\|c\|cpp\)$')
while read -r CPP_FILE_PATH; do
    # Skip unwanted files
    if [[ "$CPP_FILE_PATH" == "./.old"* ]]; then continue; fi
    if [[ "$CPP_FILE_PATH" == "./build"* ]]; then continue; fi
    if [[ "$CPP_FILE_PATH" == "./core/libcorrect"* ]]; then continue; fi
    if [[ "$CPP_FILE_PATH" == "./core/std_replacement"* ]]; then continue; fi
    if [[ "$CPP_FILE_PATH" == "./core/src/spdlog"* ]]; then continue; fi
    if [[ "$CPP_FILE_PATH" == "./core/src/imgui"* ]]; then continue; fi
    if [[ "$CPP_FILE_PATH" == "./misc_modules/discord_integration/discord-rpc"* ]]; then continue; fi
    if [[ "$CPP_FILE_PATH" == "./source_modules/sddc_source/src/libsddc"* ]]; then continue; fi
    
    if [ "$CPP_FILE_PATH" = ./core/src/json.hpp ]; then continue; fi
    if [ "$CPP_FILE_PATH" = ./core/src/gui/file_dialogs.h ]; then continue; fi

    echo Formatting $CPP_FILE_PATH
    clang-format --style=file -i $CPP_FILE_PATH

    TOTAL_LINES=$(( $TOTAL_LINES + $(wc -l < "$CPP_FILE_PATH") ))
    FILE_COUNT=$(( $FILE_COUNT + 1 ))
done <<< "$CODE_FILES"

echo Lines: $TOTAL_LINES
echo File Count: $FILE_COUNT