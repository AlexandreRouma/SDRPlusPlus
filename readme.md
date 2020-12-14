# SDR++, The bloat-free SDR software

![Screenshot](https://i.imgur.com/WejsiFN.png)
SDR++ is a cross-platform and open source SDR software with the aim of being bloat free and simple to use.

* [Patreon](https://patreon.com/ryzerth)
* [Discord Server](https://discord.gg/aFgWjyD)


## Features

* Wide hardware support (both through SoapySDR and dedicated modules)
* SIMD accelerated DSP
* Cross-platform (Windows, Linux, OSX and BSD)
* Full waterfall update when possible. Makes browsing signals easier and more pleasant

## Comming soon

* Digital demodulators and decoders
* Light theme (I know you weirdos exist lol)
* Waterfall color scheme editor
* Switchable fft size
* other small customisation options


# Building on Windows

## Requirements

* cmake
* vcpkg (for the packages listed below)
* fftw3
* portaudio
* glfw
* glew
* PothosSDR (for libvolk and SoapySDR)

## The build

```powershell
mkdir build
cd build
cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"
cmake --build . --config Release
```

# Building on Linux

## install requirements

On Debian/Ubtuntu system:
apt install libglew-dev libglfw3-dev libfftw3-dev libvolk1-dev portaudio19-dev libsoapysdr-dev gcc

## The build
Replace `<N>` by the number of threads available for building (eg. 4)

```sh
mkdir build
cd build
cmake ..
make -j<N>
```

## Modify root_dev/config.json
You'll then need to set the right paths for the modules.
This is optional. If you don't plan on modifying the modules,
you can simply copy over their binaries (from `build/<module name>/<module name>.so`)
to the `root_dev/modules/` directory.

Here is an example of module paths in `root_dev/config.json`

```json
"modules": [
    "./radio/radio.so",
    "./recorder/recorder.so",
    "./audio_sink/audio_sink.so",
    "./soapy_source/soapy_source.so",
    "./plutosdr_source/plutosdr_source.so",
    "./rtl_tcp_source/rtl_tcp_source.so"
]
```

# Building on OSX
## Install requirements
```
brew tap pothosware/homebrew-pothos
brew install volk glew glfw fftw portaudio
brew install soapysdr
```
You can install additional soapy device support based on your hardware.

## The build
```
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Configuring

You'll then need to set the right paths for the modules.
This is optional. If you don't plan on modifying the modules,
you can simply copy over their binaries (from `build/<module name>/<module name>.so`)
to the `root_dev/modules/` directory.

Here is an example of module paths in `root_dev/config.json`

```json
"modules": [
    "./radio/radio.so",
    "./recorder/recorder.so",
    "./soapy/soapy.so",
    "./rtl_tcp_source/rtl_tcp_source.so"
]
```

# Building on OpenBSD
## Install requirements

NOTE: These instructions are outdated
```
pkg_add fftw3-float glew glfw portaudio-svn

# install volk and SoapySDR manually
```

## The build
```
mkdir build
cd build
cmake --clang ..
make
cd ..
./prepare_root.sh
cp -Rf root root_dev # if are in dev
mv root/modules ./
```

## Run SDRPP with `build/sdrpp`.
Modify root_dev/modules_list.json
If the content is different than the following, change it.
```
{
    "Radio": "./radio/radio.dylib",
    "Recorder": "./recorder/recorder.dylib",
    "Soapy": "./soapy/soapy.dylib",
    "RTLTCPSource": "./rtl_tcp_source/rtl_tcp_source.dylib"
}
```

# Contributing

Feel free to issue pull request and report bugs via the github issues.
I will soon publish a contributing.md listing the code style to use.

# Credits

## Patrons
* [SignalsEverywhere](https://signalseverywhere.com/)
* [Lee Donaghy](https://github.com/github)

## Contributors
* [aosync](https://github.com/aosync)
* [Benjamin Kyd](https://github.com/benkyd)
* [Tobias Mädel](https://github.com/Manawyrm)
* [Raov](https://twitter.com/raov_birbtog)
* [Howard0su](https://github.com/howard0su)

## Libaries used
* [SoapySDR (PothosWare)](https://github.com/pothosware/SoapySDR)
* [Dear ImGui (ocornut)](https://github.com/ocornut/imgui)
* [spdlog (gabime)](https://github.com/gabime/spdlog)
* [json (nlohmann)](https://github.com/nlohmann/json)
* [portaudio (PortAudio community)](http://www.portaudio.com/)
