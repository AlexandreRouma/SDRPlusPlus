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

# Installing
## Windows
Download the latest release from [the Releases page](https://github.com/AlexandreRouma/SDRPlusPlus/releases) and extract to the directory of your choice.

To create a desktop short, rightclick the exe and select `Send to -> Desktop (create shortcut)`, then, rename the shortcut on the desktop to whatever you want.

## Linux
TODO

## MacOS
TODO

## BSD
TODO

# Building on Windows
## Install dependencies
* cmake
* vcpkg
* PothosSDR (This will install libraires for most SDRs)

After this, install the following depencies using vcpkg:
* fftw3
* portaudio
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
* vcpkg
* fftw3
* glfw
* glew
* libvolk
  
Next install dependencies based on the modules you wish to build:
* soapy_source: SoapySDR + drivers for each SDRs (see SoapySDR docs)
* airspyhf_source: libairspyhf
* plutosdr_source: libiio, libad9361
* audio_sink: portaudio

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

You will first need to edit the `root_dev/config` file to point to the modules that were built. Here us a sample if what it would look like:

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
Coming soon!

# Contributing

Feel free to issue pull request and report bugs via the github issues.
I will soon publish a contributing.md listing the code style to use.

# Credits

## Patrons
* [SignalsEverywhere](https://signalseverywhere.com/)
* [Lee Donaghy](https://github.com/github)

## Contributors
* [aosync](https://github.com/aosync)
* [Alexsey Shestacov](https://github.com/wingrime)
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