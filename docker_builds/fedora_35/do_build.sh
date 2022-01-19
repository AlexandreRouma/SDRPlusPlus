#!/bin/bash
set -e
cd /root

# Install fedora dependencies and tools
dnf install -y gcc gcc-c++ rpmdevtools rpmlint cmake git glew-devel cmake fftw-devel volk-devel glfw-devel airspyone_host-devel hackrf-devel libusb-devel rtl-sdr-devel SoapySDR-devel rtaudio-devel

# Build the sdrpp rpms
# cd SDRPlusPlus
cd SDRPlusPlus
chmod +x make_fedora_package.sh
bash make_fedora_package.sh $BUILD_NO