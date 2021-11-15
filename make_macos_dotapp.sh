#!/bin/sh
set -e

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

# Gather all dependencies

# Copy core and its dependencies
cp $BUILD_DIR/sdrpp $DOTAPP/Contents/MacOS/
cp $BUILD_DIR/core/libsdrpp_core.dylib $DOTAPP/Contents/Frameworks/

# Get dep paths
LIBGLEW_PATH=$(sh macos/get_library_path.sh libGLEW $DOTAPP/Contents/MacOS/sdrpp)
LIBFFTW3F_PATH=$(sh macos/get_library_path.sh libfftw3f $DOTAPP/Contents/MacOS/sdrpp)
LIBGLFW_PATH=$(sh macos/get_library_path.sh libglfw $DOTAPP/Contents/MacOS/sdrpp)
LIBVOLK_PATH=$(sh macos/get_library_path.sh libvolk $DOTAPP/Contents/MacOS/sdrpp)

# Modify path for sdrpp
sh macos/set_library_path.sh libglew @rpath/libglew.dylib $DOTAPP/Contents/MacOS/sdrpp
sh macos/set_library_path.sh libfftw3f @rpath/libfftw3f.dylib $DOTAPP/Contents/MacOS/sdrpp
sh macos/set_library_path.sh libglfw @rpath/libglfw.dylib $DOTAPP/Contents/MacOS/sdrpp
sh macos/set_library_path.sh libvolk @rpath/libvolk.dylib $DOTAPP/Contents/MacOS/sdrpp

# Modify path for libsdrpp_core
sh macos/set_library_path.sh libglew @rpath/libglew.dylib $DOTAPP/Contents/Frameworks/libsdrpp_core.dylib
sh macos/set_library_path.sh libfftw3f @rpath/libfftw3f.dylib $DOTAPP/Contents/Frameworks/libsdrpp_core.dylib
sh macos/set_library_path.sh libglfw @rpath/libglfw.dylib $DOTAPP/Contents/Frameworks/libsdrpp_core.dylib
sh macos/set_library_path.sh libvolk @rpath/libvolk.dylib $DOTAPP/Contents/Frameworks/libsdrpp_core.dylib

# Copy deps
cp $LIBGLEW_PATH $DOTAPP/Contents/Frameworks/libglew.dylib
cp $LIBFFTW3F_PATH $DOTAPP/Contents/Frameworks/libfftw3f.dylib
cp $LIBGLFW_PATH $DOTAPP/Contents/Frameworks/libglfw.dylib
cp $LIBVOLK_PATH $DOTAPP/Contents/Frameworks/libvolk.dylib
