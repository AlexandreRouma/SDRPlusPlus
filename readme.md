# SDR++, The bloat-free SDR software.
SDR++ is a cross-platform and open source SDR software with the aim of being bloat free and simple to use.

## Current Features
* Uses SoapySDR for wide hardware support
* Hardware accelerated graphics (OpenGL + ImGui)
* SIMD accelerated DSP (parts of the DSP are still missing)
* Cross-platform
* Full waterfall update when possible. Makes browsing signals easier and more pleasant

## Comming soon
* Multi-VFO
* Plugins
* Digital demodulators and decoders

# Building on Windows
## Requirements
* cmake
* vcpkg (for the packages listed below)
* fftw3
* portaudio
* glfw
* glew
* PothosSDR (for libvolk and SoapySDR)

```
mkdir build
cd build
cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"
cmake --build . --config Release
```

# Building on Linux
comming soon :)

# Building on OSX
comming soon as well :)

# Contributing
Feel free to issue pull request and report bugs via the github issues.
I will soon publish a contributing.md listing the code style to use.