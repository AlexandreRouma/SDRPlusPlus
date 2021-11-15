#!/bin/sh
set -e

# Options
BUILD_DIR=$1
DOTAPP=$2

# Remove existing .app
rm -rf $DOTAPP

# Create .app structure
mkdir $DOTAPP
mkdir $DOTAPP/MacOS
mkdir $DOTAPP/Frameworks
mkdir $DOTAPP/Resources

# Gather all dependencies

# Copy core and its dependencies
cp $BUILD_DIR/sdrpp $DOTAPP/MacOS/
cp $BUILD_DIR/core/libsdrpp_core.dylib $DOTAPP/Frameworks/

# Get dep paths
LIBGLEW_PATH=$(sh macos/get_library_path.sh libGLEW $DOTAPP/MacOS/sdrpp)
LIBFFTW3F_PATH=$(sh macos/get_library_path.sh libfftw3f $DOTAPP/MacOS/sdrpp)
LIBGLFW_PATH=$(sh macos/get_library_path.sh libglfw $DOTAPP/MacOS/sdrpp)
LIBVOLK_PATH=$(sh macos/get_library_path.sh libvolk $DOTAPP/MacOS/sdrpp)

# Modify path for sdrpp
sh macos/set_library_path.sh libglew @rpath/libglew.dylib $DOTAPP/MacOS/sdrpp
sh macos/set_library_path.sh libfftw3f @rpath/libfftw3f.dylib $DOTAPP/MacOS/sdrpp
sh macos/set_library_path.sh libglfw @rpath/libglfw.dylib $DOTAPP/MacOS/sdrpp
sh macos/set_library_path.sh libvolk @rpath/libvolk.dylib $DOTAPP/MacOS/sdrpp

# Modify path for libsdrpp_core
sh macos/set_library_path.sh libglew @rpath/libglew.dylib $DOTAPP/Frameworks/libsdrpp_core.dylib
sh macos/set_library_path.sh libfftw3f @rpath/libfftw3f.dylib $DOTAPP/Frameworks/libsdrpp_core.dylib
sh macos/set_library_path.sh libglfw @rpath/libglfw.dylib $DOTAPP/Frameworks/libsdrpp_core.dylib
sh macos/set_library_path.sh libvolk @rpath/libvolk.dylib $DOTAPP/Frameworks/libsdrpp_core.dylib

# Copy deps
cp $LIBGLEW_PATH $DOTAPP/Frameworks/libglew.dylib
cp $LIBFFTW3F_PATH $DOTAPP/Frameworks/libfftw3f.dylib
cp $LIBGLFW_PATH $DOTAPP/Frameworks/libglfw.dylib
cp $LIBVOLK_PATH $DOTAPP/Frameworks/libvolk.dylib
