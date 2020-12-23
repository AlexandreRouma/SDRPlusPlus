#!/bin/sh

# Create directory structure
echo Create directory structure
mkdir sdrpp_deb
mkdir sdrpp_deb/DEBIAN
mkdir sdrpp_deb/usr
mkdir sdrpp_deb/usr/bin
mkdir sdrpp_deb/usr/share
mkdir sdrpp_deb/usr/lib
mkdir sdrpp_deb/usr/lib/sdrpp
mkdir sdrpp_deb/usr/lib/sdrpp/plugins

# Create package info
echo Create package info
echo Package: sdrpp >> sdrpp_deb/DEBIAN/control
echo Version: 0.2.5 >> sdrpp_deb/DEBIAN/control
echo Maintainer: Ryzerth >> sdrpp_deb/DEBIAN/control
echo Architecture: all >> sdrpp_deb/DEBIAN/control
echo Description: Bloat-free SDR receiver software >> sdrpp_deb/DEBIAN/control

# Copy core files
echo Copy core files
cp $1/sdrpp sdrpp_deb/usr/bin/
cp $1/libsdrpp_core.so sdrpp_deb/usr/lib/

# Copy reasources
echo Copy reasources
cp -r root/res/* sdrpp_deb/usr/share/sdrpp/

# Copy module
echo Copy modules
cp $1/radio/radio.so sdrpp_deb/usr/lib/sdrpp/plugins/
cp $1/recorder/recorder.so sdrpp_deb/usr/lib/sdrpp/plugins/
cp $1/airspyhf_source/airspyhf_source.so sdrpp_deb/usr/lib/sdrpp/plugins/
cp $1/rtl_tcp_source/rtl_tcp_source.so sdrpp_deb/usr/lib/sdrpp/plugins/
cp $1/soapy_source/soapy_source.so sdrpp_deb/usr/lib/sdrpp/plugins/
cp $1/audio_sink/audio_sink.so sdrpp_deb/usr/lib/sdrpp/plugins/

# Create package
echo Create package
dpkg-deb --build sdrpp_deb