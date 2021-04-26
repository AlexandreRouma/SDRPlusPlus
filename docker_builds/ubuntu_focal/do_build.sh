#!/bin/bash
set -e
cd /root

export TZ=Europe/Brussels

apt update
apt install -y build-essential cmake git libfftw3-dev libglfw3-dev libglew-dev libvolk2-dev libsoapysdr-dev libairspyhf-dev libairspy-dev libiio-dev libad9361-dev librtaudio-dev libhackrf-dev librtlsdr-dev

git clone https://github.com/AlexandreRouma/SDRPlusPlus

cd SDRPlusPlus
mkdir build
cd build
cmake ..
make -j2

cd ..
sh make_debian_package.sh ./build