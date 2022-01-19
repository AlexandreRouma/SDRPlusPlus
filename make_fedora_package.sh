#!/bin/bash

VERSION=1.0.4
BUILD_NO=$1

# Create rpmbuild directory
rpmdev-setuptree

# Create spec file
echo "Name: sdrpp" >$HOME/rpmbuild/SPECS/sdrpp.spec
echo "Version: $VERSION" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "Release: $BUILD_NO%{?dist}" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "Summary: Bloat-free SDR receiver software" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "License: GPLv3" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "URL: https://www.sdrpp.org/" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "Source0: https://github.com/AlexandreRouma/SDRPlusPlus/archive/refs/tags/%{version}.tar.gz"
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "BuildRequires: glew-devel cmake fftw-devel volk-devel glfw-devel airspyone_host-devel hackrf-devel libusb-devel rtl-sdr-devel SoapySDR-devel rtaudio-devel" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "Requires: glew cmake fftw volk glfw airspyone hackrf libusb rtl-sdr SoapySDR rtaudio" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%description" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "Bloat-free SDR receiver software" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%prep" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "rm -rf SDRPlusPlus-%{version}" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo 'cp -r ${RPM_SOURCE_DIR}/SDRPlusPlus-%{version} .' >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%build" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "rm -rf SDRPlusPlus-%{version}/build" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "mkdir -p SDRPlusPlus-%{version}/build" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "cd SDRPlusPlus-%{version}/build" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "cmake -DOPT_BUILD_AIRSPYHF_SOURCE=OFF -DOPT_BUILD_PLUTOSDR_SOURCE=OFF .." >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "make" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%install" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "cd SDRPlusPlus-%{version}/build" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%make_install" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%files" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%license SDRPlusPlus-%{version}/license" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/bin/sdrpp" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/libsdrpp_core.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/airspy_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/audio_sink.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/discord_integration.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/file_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/frequency_manager.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/hackrf_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/meteor_demodulator.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/network_sink.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/radio.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/recorder.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/rigctl_server.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/rtl_sdr_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/rtl_tcp_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/soapy_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/spyserver_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/lib/sdrpp/plugins/rfspace_source.so" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/applications/sdrpp.desktop" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/austria.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/canada.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/china.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/france.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/general.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/germany-mobile-lte-bands.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/germany-mobile-networks.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/germany.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/russia.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/united-kingdom.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/bandplans/usa.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/classic.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/classic_green.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/electric.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/gqrx.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/greyscale.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/inferno.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/magma.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/plasma.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/turbo.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/viridis.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/colormaps/websdr.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/fonts/Roboto-Medium.ttf" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/center_tuning.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/menu.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/muted.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/normal_tuning.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/play.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/sdrpp.ico" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/sdrpp.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/stop.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/icons/unmuted.png" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo '"/usr/share/sdrpp/themes/army green.json"' >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/themes/dark.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo '"/usr/share/sdrpp/themes/deep blue.json"' >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/themes/grey.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "/usr/share/sdrpp/themes/light.json" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "%changelog" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "* $(date +"%a %b %d %Y") Joshua Mulliken <joshua@mulliken.net> - 1.0.4-$BUILD_NO" >>$HOME/rpmbuild/SPECS/sdrpp.spec
echo "- Initial RPM release" >>$HOME/rpmbuild/SPECS/sdrpp.spec

# Copy source to rpm build sources directory
mkdir -p $HOME/rpmbuild/SOURCES/SDRPlusPlus-$VERSION
cp -r ./* $HOME/rpmbuild/SOURCES/SDRPlusPlus-$VERSION/

# Build the rpm
rpmbuild -ba $HOME/rpmbuild/SPECS/sdrpp.spec

# Rename the rpm so it can be retrieved
mv $HOME/rpmbuild/RPMS/x86_64/sdrpp-$VERSION-$BUILD_NO.fc35.x86_64.rpm $HOME/rpmbuild/RPMS/x86_64/sdrpp.f35.x86_64.rpm