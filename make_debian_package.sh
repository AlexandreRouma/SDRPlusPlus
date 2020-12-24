#!/bin/sh

# Create directory structure
echo Create directory structure
mkdir sdrpp_debian_amd64
mkdir sdrpp_debian_amd64/DEBIAN
mkdir sdrpp_debian_amd64/usr
mkdir sdrpp_debian_amd64/usr/bin
mkdir sdrpp_debian_amd64/usr/share
mkdir sdrpp_debian_amd64/usr/share/sdrpp
mkdir sdrpp_debian_amd64/usr/lib
mkdir sdrpp_debian_amd64/usr/lib/sdrpp
mkdir sdrpp_debian_amd64/usr/lib/sdrpp/plugins

# Create package info
echo Create package info
echo Package: sdrpp >> sdrpp_debian_amd64/DEBIAN/control
echo Version: 0.2.5 >> sdrpp_debian_amd64/DEBIAN/control
echo Maintainer: Ryzerth >> sdrpp_debian_amd64/DEBIAN/control
echo Architecture: all >> sdrpp_debian_amd64/DEBIAN/control
echo Description: Bloat-free SDR receiver software >> sdrpp_debian_amd64/DEBIAN/control

# Copy core files
echo Copy core files
cp $1/sdrpp sdrpp_debian_amd64/usr/bin/
cp $1/libsdrpp_core.so sdrpp_debian_amd64/usr/lib/

# Copy reasources
echo Copy reasources
cp -r root/res/* sdrpp_debian_amd64/usr/share/sdrpp/

# Copy module
echo Copy modules
cp $1/radio/radio.so sdrpp_debian_amd64/usr/lib/sdrpp/plugins/
cp $1/recorder/recorder.so sdrpp_debian_amd64/usr/lib/sdrpp/plugins/
cp $1/airspyhf_source/airspyhf_source.so sdrpp_debian_amd64/usr/lib/sdrpp/plugins/
cp $1/plutosdr_source/plutosdr_source.so sdrpp_debian_amd64/usr/lib/sdrpp/plugins/
cp $1/rtl_tcp_source/rtl_tcp_source.so sdrpp_debian_amd64/usr/lib/sdrpp/plugins/
cp $1/soapy_source/soapy_source.so sdrpp_debian_amd64/usr/lib/sdrpp/plugins/
cp $1/audio_sink/audio_sink.so sdrpp_debian_amd64/usr/lib/sdrpp/plugins/

# Create package
echo Create packagesudo
dpkg-deb --build sdrpp_debian_amd64

# Cleanup
echo Cleanup
rm -rf sdrpp_debian_amd64