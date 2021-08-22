#!/bin/sh

# Create directory structure
echo Create directory structure
mkdir sdrpp_debian_amd64
mkdir sdrpp_debian_amd64/DEBIAN

# Create package info
echo Create package info
echo Package: sdrpp >> sdrpp_debian_amd64/DEBIAN/control
echo Version: 1.0.4$BUILD_NO >> sdrpp_debian_amd64/DEBIAN/control
echo Maintainer: Ryzerth >> sdrpp_debian_amd64/DEBIAN/control
echo Architecture: all >> sdrpp_debian_amd64/DEBIAN/control
echo Description: Bloat-free SDR receiver software >> sdrpp_debian_amd64/DEBIAN/control
echo Depends: $2 >> sdrpp_debian_amd64/DEBIAN/control

# Copying files
ORIG_DIR=$PWD
cd $1
make install DESTDIR=$ORIG_DIR/sdrpp_debian_amd64
cd $ORIG_DIR

# Create package
echo Create package
dpkg-deb --build sdrpp_debian_amd64

# Cleanup
echo Cleanup
rm -rf sdrpp_debian_amd64
