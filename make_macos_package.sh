#!/bin/sh

mkdir sdrpp

# Copy files
ORIG_DIR=$PWD
cd $1
make install DESTDIR=$ORIG_DIR/sdrpp
cd $ORIG_DIR

# Create archive
productbuild --root sdrpp / sdrpp_macos_amd64.pkg

# Cleanup
rm -rf sdrpp/