# SDR++, The bloat-free SDR software

![Screenshot](https://i.imgur.com/EFOqwQQ.png)
SDR++ is a cross-platform and open source SDR software with the aim of being bloat free and simple to use.

![Build](https://github.com/AlexandreRouma/SDRPlusPlus/workflows/Build%20Binaries/badge.svg)

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

To create a desktop shortcut, rightclick the exe and select `Send to -> Desktop (create shortcut)`, then, rename the shortcut on the desktop to whatever you want.

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

You will first need to edit the `root_dev/config.json` file to point to the modules that were built. Here is an example of what it should look like:

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

Of course, remember to add entries for all modules that were built and that you wish to use.

Next, from the top directory, you can simply run:
```
./build/Release/sdrpp.exe -r root_dev
```

Or, if you wish to run from the build directory:
```
./Release/sdrpp.exe -r ../root_dev
```

## Installing SDR++
If you choose to run SDR++ for development, you do not need this step.
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
## Select which modules you wish to build
Depending on which module you want to build, you will need to install some additional dependencies.
Here are listed every module that requires addition dependencies. If a module enabled by default and you do not wish to install a perticular dependency (or can't, eg. the BladeRF module on Debian Buster),
you can disable it using the module parameter listed in the table below

* soapy_source: SoapySDR + drivers for each SDRs (see SoapySDR docs)
* airspyhf_source: libairspyhf
* plutosdr_source: libiio, libad9361
* audio_sink: librtaudio-dev

## Install dependencies
* cmake
* fftw3
* glfw
* glew
* libvolk

Next install dependencies based on the modules you wish to build (See previous step)

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

Then, you will need to edit the `root_dev/config.json` file to point to the modules that were built. Here is an example of what it should look like:

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

Or, if you wish to run from the build directory, you will need to correct the directories in the config.json file, and then run:
```
./sdrpp -r ../root_dev
```

## Installing SDR++
To install SDR++, run the following command in your ``build`` folder:
```sh
sudo make install
```

# Module List

Not all modules are built by default. I decided to disable the build of those with large libraries, libraries that can't be installed through the package manager (or pothos) and those that are still in beta.
Modules in beta are still included in releases for the most part but not enabled in SDR++ (need to be instantiated).

## Sources

| Name             | Stage      | Dependencies      | Option                     | Built by default| Built in Release        | Enabled in SDR++ by default |
|------------------|------------|-------------------|----------------------------|:---------------:|:-----------------------:|:---------------------------:|
| airspy_source    | Working    | libairspy         | OPT_BUILD_AIRSPY_SOURCE    | ✅              | ✅                     | ✅                         |
| airspyhf_source  | Working    | libairspyhf       | OPT_BUILD_AIRSPYHF_SOURCE  | ✅              | ✅                     | ✅                         |
| bladerf_source   | Beta       | libbladeRF        | OPT_BUILD_BLADERF_SOURCE   | ⛔              | ⚠️ (not Debian Buster) | ✅                         |
| file_source      | Working    | -                 | OPT_BUILD_FILE_SOURCE      | ✅              | ✅                     | ✅                         |
| hackrf_source    | Working    | libhackrf         | OPT_BUILD_HACKRF_SOURCE    | ✅              | ✅                     | ✅                         |
| limesdr_source   | Beta       | liblimesuite      | OPT_BUILD_LIMESDR_SOURCE   | ⛔              | ✅                     | ✅                         |
| sddc_source      | Unfinished | -                 | OPT_BUILD_SDDC_SOURCE      | ⛔              | ⛔                     | ⛔                         |
| rtl_sdr_source   | Working    | librtlsdr         | OPT_BUILD_RTL_SDR_SOURCE   | ✅              | ✅                     | ✅                         |
| rtl_tcp_source   | Working    | -                 | OPT_BUILD_RTL_TCP_SOURCE   | ✅              | ✅                     | ✅                         |
| sdrplay_source   | Working    | SDRplay API       | OPT_BUILD_SDRPLAY_SOURCE   | ⛔              | ✅                     | ✅                         |
| soapy_source     | Working    | soapysdr          | OPT_BUILD_SOAPY_SOURCE     | ✅              | ✅                     | ✅                         |
| spyserver_source | Unfinished | -                 | OPT_BUILD_SPYSERVER_SOURCE | ⛔              | ⛔                     | ⛔                         |
| plutosdr_source  | Working    | libiio, libad9361 | OPT_BUILD_PLUTOSDR_SOURCE  | ✅              | ✅                     | ✅                         |

## Sinks

| Name             | Stage      | Dependencies      | Option                     | Built by default| Built in Release        | Enabled in SDR++ by default |
|------------------|------------|-------------------|----------------------------|:---------------:|:-----------------------:|:---------------------------:|
| audio_sink       | Working    | rtaudio           | OPT_BUILD_AUDIO_SINK       | ✅              | ✅                     | ✅                         |

## Decoders

| Name                | Stage      | Dependencies | Option                        | Built by default| Built in Release        | Enabled in SDR++ by default |
|---------------------|------------|--------------|-------------------------------|:---------------:|:-----------------------:|:---------------------------:|
| falcon9_decoder     | Beta       | ffplay       | OPT_BUILD_FALCON9_DECODER     | ⛔              | ⛔                     | ⛔                         |
| meteor_demodulator  | Working    | -            | OPT_BUILD_METEOR_DEMODULATOR  | ✅              | ✅                     | ⛔                         |
| radio               | Working    | -            | OPT_BUILD_RADIO               | ✅              | ✅                     | ✅                         |
| weather_sat_decoder | Unfinished | -            | OPT_BUILD_WEATHER_SAT_DECODER | ⛔              | ⛔                     | ⛔                         |

## Misc

| Name                | Stage      | Dependencies | Option                        | Built by default| Built in Release        | Enabled in SDR++ by default |
|---------------------|------------|--------------|-------------------------------|:---------------:|:-----------------------:|:---------------------------:|
| discord_integration | Working    | -            | OPT_BUILD_DISCORD_PRESENCE    | ✅              | ✅                     | ⛔                         |
| frequency_manager   | Beta       | -            | OPT_BUILD_FREQUENCY_MANAGER   | ✅              | ✅                     | ✅                         |
| recorder            | Working    | -            | OPT_BUILD_RECORDER            | ✅              | ✅                     | ✅                         |

# Toubleshooting

First, please make sure you're running the latest automated build. If your issue is linked to a bug it is likely that is has already been fixed in later releases

## "hash collision" error when starting

You likely installed the `soapysdr-module-all` package on Ubuntu/Debian. If not it's still a SoapySDR bug caused by multiple soapy modules comming in conflict. Uninstall anything related to SoapySDR then install soapysdr itself and only the soapy modules you actually need.

## "I don't see -insert module name here-, what's going on?"

If the module was included in a later update, it's not enabled in the config. The easiest way to fix this is just to delete the `config.json` file and let SDR++ recreate it (you will lose your setting relating to the main UI like VFO colors, zoom level and theme).
The best option however is to edit the config file to add an instance of the module you wish to hae enabled (see the Module List).

## SDR++ crashes when stopping a RTL-SDR

This is a bug recently introduced by libusb1.4
To solve, this, simply downgrade to libusb1.3

## SDR++ crashes when starting a HackRF

If you also have the SoapySDR module loaded (not necessarily enabled), this is a bug in libhackrf. It's caused by libhackrf not checking if it's already initialized. 
The solution until a fixed libhackrf version is released is to completely remove the soapy_source module from SDR++. To do this, delete `modules/soapy_source.dll` on windows
or `/usr/share/sdrpp/plugins/soapy_source.so` on linux.

## Issue not listed here?

If you still have an issue, please open an issue about it or ask on the discord. I'll try to respond as quickly as I can. Please avoid trying to contact me on every platform imaginable thinking I'll respond faster though...

# Contributing

Feel free to submit pull requests and report bugs via the GitHub issue tracker.
I will soon publish a contributing.md listing the code style to use.

# Credits

## Patrons

* [Croccydile](https://example.com/)
* [Daniele D'Agnelli](https://linkedin.com/in/dagnelli)
* [W4IPA](https://twitter.com/W4IPAstroke5)
* [Lee Donaghy](https://github.com/github)
* [ON4MU](https://github.com/)
* [Passion-Radio.com](https://passion-radio.com/)
* [Scanner School](https://scannerschool.com/)
* [SignalsEverywhere](https://signalseverywhere.com/)


## Contributors
* [Aang23](https://github.com/Aang23)
* [Alexsey Shestacov](https://github.com/wingrime)
* [Aosync](https://github.com/aosync)
* [Benjamin Kyd](https://github.com/benkyd)
* [Benjamin Vernoux](https://github.com/bvernoux)
* [Cropinghigh](https://github.com/cropinghigh)
* [Fred F4EED](http://f4eed.wordpress.com/)
* [Howard0su](https://github.com/howard0su)
* [Joshua Kimsey](https://github.com/JoshuaKimsey)
* [Martin Hauke](https://github.com/mnhauke)
* [Marvin Sinister](https://github.com/marvin-sinister)
* [Paulo Matias](https://github.com/thotypous)
* [Raov](https://twitter.com/raov_birbtog)
* [Starman0620](https://github.com/Starman0620)
* [Syne Ardwin (WI9SYN)](https://esaille.me/)
* [Szymon Zakrent](https://github.com/zakrent)
* [Tobias Mädel](https://github.com/Manawyrm)
* [Zimm](https://github.com/invader-zimm)


## Libraries used
* [SoapySDR (PothosWare)](https://github.com/pothosware/SoapySDR)
* [Dear ImGui (ocornut)](https://github.com/ocornut/imgui)
* [spdlog (gabime)](https://github.com/gabime/spdlog)
* [json (nlohmann)](https://github.com/nlohmann/json)
* [rtaudio](http://www.portaudio.com/)
* [Portable File Dialogs](https://github.com/samhocevar/portable-file-dialogs)
