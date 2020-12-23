#!/bin/sh

# Create directory structure
echo Create directory structure
mkdir sdrpp
mkdir sdrpp/DEBIAN
mkdir sdrpp/usr
mkdir sdrpp/usr/bin
mkdir sdrpp/usr/share
mkdir sdrpp/usr/lib
mkdir sdrpp/usr/lib/sdrpp
mkdir sdrpp/usr/lib/sdrpp/plugins

# Create package info
echo Create package info
echo Package: sdrpp >> sdrpp/DEBIAN/control
echo Version: 0.2.5 >> sdrpp/DEBIAN/control
echo Maintainer: Ryzerth >> sdrpp/DEBIAN/control
echo Architecture: all >> sdrpp/DEBIAN/control
echo Description: Bloat-free SDR receiver software >> sdrpp/DEBIAN/control

# Copy core files
echo Copy core files
cp build/sdrpp sdrpp/usr/bin/
cp build/libsdrpp_core.so sdrpp/usr/lib/

# Copy reasources
echo Copy reasources
cp -r root/res/* sdrpp/usr/bin/

# Copy module
echo Copy modules
cp build/radio/radio.so sdrpp/usr/lib/sdrpp/plugins/
cp build/recorder/recorder.so sdrpp/usr/lib/sdrpp/plugins/
cp build/airspyhf_source/airspyhf_source.so sdrpp/usr/lib/sdrpp/plugins/
cp build/rtl_tcp_source/rtl_tcp_source.so sdrpp/usr/lib/sdrpp/plugins/
cp build/soapy_source/soapy_source.so sdrpp/usr/lib/sdrpp/plugins/
cp build/audio_sink/audio_sink.so sdrpp/usr/lib/sdrpp/plugins/

# Create package
echo Create package
dpkg-deb --build sdrpp