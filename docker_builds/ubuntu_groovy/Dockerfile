FROM ubuntu:groovy

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt full-upgrade -y && \
    apt install -y --no-install-recommends build-essential ca-certificates cmake git libfftw3-dev libglfw3-dev libglew-dev libvolk2-dev libsoapysdr-dev libairspyhf-dev libairspy-dev libiio-dev libad9361-dev librtaudio-dev libhackrf-dev librtlsdr-dev libbladerf-dev liblimesuite-dev p7zip-full wget

COPY do_build.sh /root

RUN chmod +x /root/do_build.sh
