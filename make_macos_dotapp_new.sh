#!/bin/sh
set -e

# ========================= Prepare dotapp structure =========================

# Options
BUILD_DIR=$1
DOTAPP=$2

# Remove existing .app
rm -rf $DOTAPP

# Create .app structure
mkdir $DOTAPP
mkdir $DOTAPP/Contents
mkdir $DOTAPP/Contents/MacOS
mkdir $DOTAPP/Contents/Frameworks
mkdir $DOTAPP/Contents/Resources
mkdir $DOTAPP/Contents/Plugins

# ========================= Boilerplate =========================

# get_first_element [elem0] [elem1] [elem2] ...
get_first_element() {
    echo $1
}

# get_second_element [elem0] [elem1] [elem2] ...
get_second_element() {
    echo $2
}

# is_not_to_be_installed [dylib_name]
is_not_to_be_installed() {
    if [ "$1" = "libsdrpp_core.dylib" ]; then echo 1; fi
    if [ "$1" = "OpenGL" ]; then echo 1; fi
    if [ "$1" = "libc++.1.dylib" ]; then echo 1; fi
    if [ "$1" = "libSystem.B.dylib" ]; then echo 1; fi
    if [ "$1" = "Cocoa" ]; then echo 1; fi
    if [ "$1" = "IOKit" ]; then echo 1; fi
    if [ "$1" = "CoreFoundation" ]; then echo 1; fi
    if [ "$1" = "AppKit" ]; then echo 1; fi
    if [ "$1" = "CoreGraphics" ]; then echo 1; fi
    if [ "$1" = "CoreServices" ]; then echo 1; fi
    if [ "$1" = "Foundation" ]; then echo 1; fi
    if [ "$1" = "CoreAudio" ]; then echo 1; fi
    if [ "$1" = "AudioToolbox" ]; then echo 1; fi
    if [ "$1" = "AudioUnit" ]; then echo 1; fi
    if [ "$1" = "libobjc.A.dylib" ]; then echo 1; fi
}

# get_exec_deps [exec_path]
get_exec_deps() {
    DEPS_RAW=$(otool -L $1 | tail -n +2)

    # Iterate over all lines
    echo "$DEPS_RAW" | while read -r DEP; do
        echo $(get_first_element $DEP)
    done
}

# get_exec_rpaths [exec_path]
get_exec_rpaths() {
    RPATHS_RAW=$(otool -l $1 | grep "path\ ")

    # Iterate over all lines
    echo "$RPATHS_RAW" | while read -r RPATH; do
        echo $(get_second_element $RPATH)
    done
}

# install_exec [exec_path] [install_path]
install_exec() {
    local EXEC_NAME=$(basename $2)
    local EXEC_DEST=$1/$EXEC_NAME

    # Check if file exists
    if [ ! -f $2 ]; then
        echo "==NOT== Installing" $EXEC_NAME
        return
    fi

    echo "Installing" $EXEC_NAME

    # Copy it to its install location
    cp $2 $EXEC_DEST
    
    # Install dependencies and change path
    local DEPS=$(get_exec_deps $EXEC_DEST)
    echo "$DEPS" | while read -r DEP; do
        local DEP_NAME=$(basename $DEP)

        # Skip if this dep is blacklisted
        local NOT_TO_BE_INSTALLED=$(is_not_to_be_installed $DEP_NAME)
        if [ "$NOT_TO_BE_INSTALLED" = "1" ]; then
            continue
        fi

        # Skip if this dep is itself
        if [ "$DEP_NAME" = "$EXEC_NAME" ]; then
            continue
        fi

        # If the dependency is not installed, install it
        if [ ! -f $DOTAPP/Contents/Frameworks/$DEP_NAME ]; then
            install_exec $DOTAPP/Contents/Frameworks $DEP
        fi

        # Fix path
        install_name_tool -change $DEP @rpath/$DEP_NAME $EXEC_DEST
    done

    # Remove all its rpaths
    local RPATHS=$(get_exec_rpaths $EXEC_DEST)
    if [ "$RPATHS" != "" ]; then
        echo "$RPATHS" | while read -r RPATH; do
            install_name_tool -delete_rpath $RPATH $EXEC_DEST
        done
    fi
    
    # Add new single rpath
    install_name_tool -add_rpath @loader_path/../Frameworks $EXEC_DEST

    # Re-sign
    codesign --remove-signature $EXEC_DEST
    codesign -s - $EXEC_DEST
}

# ========================= Install binaries =========================

# Core
install_exec $DOTAPP/Contents/MacOS $BUILD_DIR/sdrpp 
install_exec $DOTAPP/Contents/Frameworks $BUILD_DIR/core/libsdrpp_core.dylib

# Source modules
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/airspy_source/airspy_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/airspyhf_source/airspyhf_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/bladerf_source/bladerf_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/file_source/file_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/hackrf_source/hackrf_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/limesdr_source/limesdr_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/plutosdr_source/plutosdr_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/rtl_sdr_source/rtl_sdr_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/rtl_tcp_source/rtl_tcp_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/sdrplay_source/sdrplay_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/soapy_source/soapy_source.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/source_modules/spyserver_source/spyserver_source.dylib

# Sink modules
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/sink_modules/portaudio_sink/audio_sink.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/sink_modules/network_sink/network_sink.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/sink_modules/new_portaudio_sink/new_portaudio_sink.dylib

# Decoder modules
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/decoder_modules/m17_decoder/m17_decoder.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/decoder_modules/meteor_demodulator/meteor_demodulator.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/decoder_modules/radio/radio.dylib

install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/misc_modules/discord_integration/discord_integration.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/misc_modules/frequency_manager/frequency_manager.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/misc_modules/recorder/recorder.dylib
install_exec $DOTAPP/Contents/Plugins $BUILD_DIR/misc_modules/rigctl_server/rigctl_server.dylib

# ========================= Install resources =========================

# Copy resource folder
cp -R root/res/* $DOTAPP/Contents/Resources/

# Generate icon
mkdir $DOTAPP/Contents/Resources/sdrpp.iconset
sips -z 16 16     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_16x16.png
sips -z 32 32     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_16x16@2x.png
sips -z 32 32     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_32x32.png
sips -z 64 64     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_32x32@2x.png
sips -z 128 128   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_128x128.png
sips -z 256 256   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_128x128@2x.png
sips -z 256 256   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_256x256.png
sips -z 512 512   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_256x256@2x.png
sips -z 512 512   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_512x512.png
iconutil -c icns $DOTAPP/Contents/Resources/sdrpp.iconset
rm -R $DOTAPP/Contents/Resources/sdrpp.iconset

# ========================= Generate the plist =========================

echo '<?xml version="1.0" encoding="UTF-8"?>' >> $DOTAPP/Contents/Info.plist
echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $DOTAPP/Contents/Info.plist
echo '' >> $DOTAPP/Contents/Info.plist
echo '<plist version="1.0">' >> $DOTAPP/Contents/Info.plist
echo '    <dict>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleName</key><string>sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleDisplayName</key><string>SDR++</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleIdentifier</key><string>org.sdrpp.sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleVersion</key><string>1.0.5</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundlePackageType</key><string>APPL</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleSignature</key><string>sdrp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleExecutable</key><string>sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleIconFile</key><string>sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '    </dict>' >> $DOTAPP/Contents/Info.plist
echo '</plist>' >> $DOTAPP/Contents/Info.plist