#!/bin/bash
set -e
cd /root

# Install dependencies and tools
dnf update -y
dnf install -y make gcc g++ cmake git wget p7zip fftw-devel glfw-devel volk-devel libzstd-devel codec2-devel airspyone_host-devel rtaudio-devel \
            hackrf-devel rtl-sdr-devel portaudio-devel spdlog-devel libusbg-devel

# Install SDRPlay libraries
wget https://www.sdrplay.com/software/SDRplay_RSP_API-Linux-3.15.1.run
7za x ./SDRplay_RSP_API-Linux-3.15.1.run
7za x ./SDRplay_RSP_API-Linux-3.15.1
cp x86_64/libsdrplay_api.so.3.15 /usr/lib/libsdrplay_api.so
cp inc/* /usr/include/

# Install librfnm
git clone https://github.com/AlexandreRouma/librfnm
cd librfnm
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make -j2
make install
cd ../../

cd SDRPlusPlus
mkdir build
cd build
cmake .. -DOPT_BUILD_AIRSPYHF_SOURCE=OFF -DOPT_BUILD_PLUTOSDR_SOURCE=OFF -DOPT_BUILD_RFNM_SOURCE=ON -DOPT_BUILD_SDRPLAY_SOURCE=ON -DOPT_BUILD_NEW_PORTAUDIO_SINK=ON -DOPT_BUILD_M17_DECODER=ON
make VERBOSE=1 -j2