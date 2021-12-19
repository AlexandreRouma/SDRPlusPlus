#!/bin/bash

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

    echo Checking $CPP_FILE_PATH
    clang-format --style=file -i -n -Werror $CPP_FILE_PATH
done <<< "$CODE_FILES"