#!/bin/bash
set -e
cd /root

# Install dependencies and tools
apt update
apt install -y build-essential cmake git libfftw3-dev libglfw3-dev libglew-dev libvolk2-dev libsoapysdr-dev libairspyhf-dev libairspy-dev \
            libiio-dev libad9361-dev librtaudio-dev libhackrf-dev librtlsdr-dev libbladerf-dev liblimesuite-dev p7zip-full wget portaudio19-dev \
            libcodec2-dev

# Install SDRPlay libraries
wget https://www.sdrplay.com/software/SDRplay_RSP_API-Linux-3.07.1.run
7z x ./SDRplay_RSP_API-Linux-3.07.1.run
7z x ./SDRplay_RSP_API-Linux-3.07.1
cp x86_64/libsdrplay_api.so.3.07 /usr/lib/libsdrplay_api.so
cp inc/* /usr/include/

cd SDRPlusPlus
mkdir build
cd build
cmake .. -DOPT_BUILD_BLADERF_SOURCE=ON -DOPT_BUILD_LIMESDR_SOURCE=ON -DOPT_BUILD_SDRPLAY_SOURCE=ON -DOPT_BUILD_NEW_PORTAUDIO_SINK=ON -DOPT_BUILD_M17_DECODER=ON
make -j2

cd ..
sh make_debian_package.sh ./build 'libfftw3-dev, libglfw3-dev, libglew-dev, libvolk2-dev, librtaudio-dev'