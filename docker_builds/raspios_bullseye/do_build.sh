#!/bin/bash
set -e
cd /root

# Install tools
apt update
apt install -y wget p7zip-full qemu-user-static gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf build-essential cmake pkg-config

# Download and extract raspberry pi image
wget https://downloads.raspberrypi.org/raspios_lite_armhf/images/raspios_lite_armhf-2021-11-08/2021-10-30-raspios-bullseye-armhf-lite.zip
7z x 2021-10-30-raspios-bullseye-armhf-lite.zip
7z x 2021-10-30-raspios-bullseye-armhf-lite.img

# Expand and mount rootfs image
dd if=/dev/zero bs=1G count=1 >> 1.img
e2fsck -p -f 1.img
resize2fs 1.img
mount 1.img /mnt

# Copy qemu to the rootfs
cp /usr/bin/qemu-arm-static /mnt/bin/

# Inject script to install dependencies
echo 'export DEBIAN_FRONTEND=noninteractive' >> /mnt/root/prepare.sh
echo 'apt update --allow-releaseinfo-change' >> /mnt/root/prepare.sh
echo 'apt install -y build-essential cmake git libfftw3-dev libglfw3-dev libvolk2-dev libsoapysdr-dev libairspyhf-dev libairspy-dev \' >> /mnt/root/prepare.sh
echo '            libiio-dev libad9361-dev librtaudio-dev libhackrf-dev librtlsdr-dev libbladerf-dev liblimesuite-dev p7zip-full wget portaudio19-dev \' >> /mnt/root/prepare.sh
echo '            libcodec2-dev' >> /mnt/root/prepare.sh

# Run prepare.sh script
chroot /mnt /bin/qemu-arm-static /bin/bash /root/prepare.sh

# Setup environment variables
export PKG_CONFIG_PATH=''
export PKG_CONFIG_LIBDIR='/mnt/usr/lib/arm-linux-gnueabihf/pkgconfig:/mnt/usr/lib/pkgconfig:/mnt/usr/share/pkgconfig'
export PKG_CONFIG_SYSROOT_DIR='/mnt'

# Build SDR++
cd SDRPlusPlus
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../docker_builds/raspios_bullseye/rpi_toolchain.cmake -DOPT_BUILD_BLADERF_SOURCE=ON -DOPT_BUILD_LIMESDR_SOURCE=ON -DOPT_BUILD_NEW_PORTAUDIO_SINK=ON -DOPT_BUILD_M17_DECODER=ON
make VERBOSE=1 -j2

# Create deb
cd ..
sh make_debian_package.sh ./build 'libfftw3-dev, libglfw3-dev, libvolk2-dev, librtaudio-dev'
mv sdrpp_debian_amd64.deb sdrpp_raspios_arm32.deb