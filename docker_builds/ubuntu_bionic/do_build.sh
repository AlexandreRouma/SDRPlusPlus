#!/bin/bash
set -e
cd /root

# Update repos to get a more recent cmake version
apt update
apt install -y gpg wget
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ bionic main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null
apt update

# Install dependencies and tools
apt install -y build-essential cmake git libfftw3-dev libglfw3-dev libglew-dev libvolk1-dev libsoapysdr-dev libairspy-dev \
            libiio-dev libad9361-dev librtaudio-dev libhackrf-dev librtlsdr-dev libbladerf-dev liblimesuite-dev p7zip-full wget portaudio19-dev \
            libcodec2-dev

# Install SDRPlay libraries
wget https://www.sdrplay.com/software/SDRplay_RSP_API-Linux-3.07.1.run
7z x ./SDRplay_RSP_API-Linux-3.07.1.run
7z x ./SDRplay_RSP_API-Linux-3.07.1
cp x86_64/libsdrplay_api.so.3.07 /usr/lib/libsdrplay_api.so
cp inc/* /usr/include/

# Install a more recent libairspyhf version
git clone https://github.com/airspy/airspyhf
cd airspyhf
mkdir build
cd build
cmake .. -DINSTALL_UDEV_RULES=ON
make -j2
make install
ldconfig
cd ../../

# Build SDR++ Itself
cd SDRPlusPlus
mkdir build
cd build
cmake .. -DOPT_BUILD_SDRPLAY_SOURCE=ON -DOPT_BUILD_BLADERF_SOURCE=OFF -DOPT_BUILD_LIMESDR_SOURCE=ON -DOPT_BUILD_NEW_PORTAUDIO_SINK=ON -DOPT_OVERRIDE_STD_FILESYSTEM=ON -DOPT_BUILD_M17_DECODER=ON
make -j2

# Generate package
cd ..
sh make_debian_package.sh ./build 'libfftw3-dev, libglfw3-dev, libglew-dev, libvolk1-dev, librtaudio-dev'