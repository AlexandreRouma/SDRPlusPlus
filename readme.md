# SDR++, The bloat-free SDR software

![Screenshot](https://i.imgur.com/EFOqwQQ.png)
SDR++ is a cross-platform and open source SDR software with the aim of being bloat free and simple to use.

![Linux Build](https://github.com/AlexandreRouma/SDRPlusPlus/workflows/Linux%20Build/badge.svg)

* [Patreon](https://patreon.com/ryzerth)
* [Discord Server](https://discord.gg/aFgWjyD)


## Features

* Wide hardware support (both through SoapySDR and dedicated modules)
* SIMD accelerated DSP
* Cross-platform (Windows, Linux, OSX and BSD)
* Full waterfall update when possible. Makes browsing signals easier and more pleasant

## Coming soon

* Digital demodulators and decoders
* Light theme (I know you weirdos exist lol)
* Waterfall color scheme editor
* Other small customisation options

# Installing
## Windows
Download the latest release from [the Releases page](https://github.com/AlexandreRouma/SDRPlusPlus/releases) and extract to the directory of your choice.

To create a desktop short, rightclick the exe and select `Send to -> Desktop (create shortcut)`, then, rename the shortcut on the desktop to whatever you want.

## Linux

### Debian-based (Ubuntu, Mint, etc)
Download the latest release from [the Releases page](https://github.com/AlexandreRouma/SDRPlusPlus/releases) and extract to the directory of your choice.

Then, run:
```sh
sudo apt install libfftw3-dev libglfw3-dev libglew-dev libvolk2-dev libsoapysdr-dev libairspyhf-dev libiio-dev libad9361-dev librtaudio-dev libhackrf-dev
sudo dpkg -i sdrpp_debian_amd64.deb
```

If `libvolk2-dev` is not available, use `libvolk1-dev`.

### Arch-based
Install the latest release from the [sdrpp-git](https://aur.archlinux.org/packages/sdrpp-git/) AUR package

### Other
There are currently no existing packages for other distributions, for these systems you'll have to [build from source](https://github.com/AlexandreRouma/SDRPlusPlus#building-on-linux--bsd).

## MacOS
TODO

## BSD
There are currently no BSD packages, refer to [Building on Linux / BSD](https://github.com/AlexandreRouma/SDRPlusPlus#building-on-linux--bsd) for instructions on building from source.

# Building on Windows
## Install dependencies
* cmake
* vcpkg
* PothosSDR (This will install libraries for most SDRs)
* rtaudio

After this, install the following dependencies using vcpkg:
* fftw3
* glfw
* glew

## Building
```
mkdir build
cd build
cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"
cmake --build . --config Release
```

## Create a new root directory
```
./create_root.bat
```

## Running for development
If you wish to install SDR++, skip to the next step

You will first need to edit the `root_dev/config` file to point to the modules that were built. Here us a sample if what it would look like:

```json
...
"modules": [
    "./build/radio/Release/radio.dll",
    "./build/recorder/Release/recorder.dll",
    "./build/rtl_tcp_source/Release/rtl_tcp_source.dll",
    "./build/soapy_source/Release/soapy_source.dll",
    "./build/audio_sink/Release/audio_sink.dll"
]
...
```

You also need to change the location of the resource and module directories, for development, I recommend:
```json
...
"modulesDirectory": "../root_dev/modules",
...
"resourcesDirectory": "../root_dev/res",
...
```

Remember that these paths will be relative to the run directory.

Off cours, remember to add entries for all modules that were built and that you wish to use.

Next, from the top directory, you can simply run:
```
./build/Release/sdrpp.exe -r root_dev
```

Or, if you wish to run from the build directory:
```
./Release/sdrpp.exe -r ../root_dev
```

## Installing SDR++
If you chose to run SDR++ for development, you do not need this step.
First, copy over the exe and DLLs from `build/Release/` to `root_dev`.

Next you need to copy over all the modules that were compiled. To do so, copy the DLL file of the module (located in its build folder given below) to the `root_dev/modules` directory and other DLLs (that do not have the exact name of the modue) to the `root_dev` directory.

The modules built will be some of the following (Repeat the instructions above for all you wish to use):
* `build/radio/Release/`
* `build/recorder/Release/`
* `build/rtl_tcp_source/Release/`
* `build/spyserver_source/Release/`
* `build/soapy_source/Release/`
* `build/airspyhf_source/Release/`
* `build/plutosdr_source/Release/`
* `build/audio_sink/Release/`




# Building on Linux / BSD
## Install dependencies
* cmake
* fftw3
* glfw
* glew
* libvolk
  
Next install dependencies based on the modules you wish to build:
* soapy_source: SoapySDR + drivers for each SDRs (see SoapySDR docs)
* airspyhf_source: libairspyhf
* plutosdr_source: libiio, libad9361
* audio_sink: librtaudio-dev

Note: make sure you're using GCC 8 or later as older versions do not have `std::filesystem` built-in.

## Building
replace `<N>` with the number of threads you wish to use to build
```sh
mkdir build
cd build
cmake ..
make -j<N>
```

## Create a new root directory
```sh
sh ./create_root.sh
```

## Running for development
If you wish to install SDR++, skip to the next step

First run SDR++ from the build directory to generate a default config file
```
./sdrpp -r ../root_dev/
```

Then, you need to edit the `root_dev/config.json` file to point to the modules that were built. Here is a sample of what it should look like:

```json
...
"modules": [
    "./build/radio/radio.so",
    "./build/recorder/recorder.so",
    "./build/rtl_tcp_source/rtl_tcp_source.so",
    "./build/soapy_source/soapy_source.so",
    "./build/audio_sink/audio_sink.so"
]
...
```

Note: You can generate this list automatically by running `find . | grep '\.so' | sed 's/^/"/' | sed 's/$/",/' | sed '/sdrpp_core.so/d'` in the build directory.

You also need to change the location of the resource and module directories, for development, I recommend:
```json
...
"modulesDirectory": "./root_dev/modules",
...
"resourcesDirectory": "./root_dev/res",
...
```

Remember that these paths will be relative to the run directory.

Of course, remember to add entries for all modules that were built and that you wish to use.

Next, from the top directory, you can simply run:
```
./build/sdrpp -r root_dev
```

Or, if you wish to run from the build directory, you need to correct directories in config.json, and:
```
./sdrpp -r ../root_dev
```

## Installing SDR++
To install SDR++, run the following command in your ``build`` folder:
```sh
sudo make install
```

# Contributing

Feel free to issue pull request and report bugs via the github issues.
I will soon publish a contributing.md listing the code style to use.

# Credits

## Patrons

* [Croccydile](https://example.com/)
* [Daniele D'Agnelli](https://linkedin.com/in/dagnelli)
* [W4IPA](https://twitter.com/W4IPAstroke5)
* [Lee Donaghy](https://github.com/github)
* [Passion-Radio.com](https://passion-radio.com/)
* [Scanner School](https://scannerschool.com/)
* [SignalsEverywhere](https://signalseverywhere.com/)


## Contributors
* [Aang23](https://github.com/Aang23)
* [Alexsey Shestacov](https://github.com/wingrime)
* [Aosync](https://github.com/aosync)
* [Benjamin Kyd](https://github.com/benkyd)
* [Cropinghigh](https://github.com/cropinghigh)
* [Howard0su](https://github.com/howard0su)
* [Martin Hauke](https://github.com/mnhauke)
* [Paulo Matias](https://github.com/thotypous)
* [Raov](https://twitter.com/raov_birbtog)
* [Szymon Zakrent](https://github.com/zakrent)
* [Tobias Mädel](https://github.com/Manawyrm)


## Libaries used
* [SoapySDR (PothosWare)](https://github.com/pothosware/SoapySDR)
* [Dear ImGui (ocornut)](https://github.com/ocornut/imgui)
* [spdlog (gabime)](https://github.com/gabime/spdlog)
* [json (nlohmann)](https://github.com/nlohmann/json)
* [rtaudio](http://www.portaudio.com/)
