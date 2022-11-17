#!/bin/sh
set -e

BUILDDIR=$1
DEPENDS=$2
DISTRO=$(lsb_release -sc)
ARCH=$(dpkg --print-architecture)

# Create directory structure
PKGDIR="sdrpp_${DISTRO}_$ARCH"
echo Create directory structure
mkdir -p "$PKGDIR/DEBIAN"

# Create package info
echo Create package info
{
    echo "Package: sdrpp"
    echo "Version: 1.1.0$BUILD_NO"
    echo "Maintainer: Ryzerth"
    echo "Architecture: $ARCH"
    echo "Description: Bloat-free SDR receiver software"
    echo "Depends: $DEPENDS"
} >> "$PKGDIR/DEBIAN/control"

# Copying files
ORIG_DIR=$PWD
cd "$BUILDDIR"
make install DESTDIR="$ORIG_DIR/$PKGDIR"
cd "$ORIG_DIR"

# Create package
echo Create package
dpkg-deb --build "$PKGDIR"

# Cleanup
echo Cleanup
rm -rf "$PKGDIR"
