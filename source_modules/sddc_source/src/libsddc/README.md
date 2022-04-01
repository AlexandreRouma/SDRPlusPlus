## ExtIO_sddc.dll (software digital down converter) - Oscar Steila, ik1xpv

![CMake](https://github.com/ik1xpv/ExtIO_sddc/workflows/CMake/badge.svg)

## Installation Instructions

You can download the latest EXTIO driver from the releases: https://github.com/ik1xpv/ExtIO_sddc/releases.
The direct link to the current version v1.2.0 Version released at 18/3/2021 is: https://github.com/ik1xpv/ExtIO_sddc/releases/download/v1.2.0/SDDC_EXTIO.ZIP.

*If you want to try the beta EXTIO driver which is for testing, you can find the binary for each change here: https://github.com/ik1xpv/ExtIO_sddc/actions. Select one specific code change you like to try, click on the link of the change. And you will find the binary on the bottom of the change.*


## Build Instructions for ExtIO

1. Install Visual Studio 2019 with Visual C++ support. You can use the free community version, which can be downloaded from: https://visualstudio.microsoft.com/downloads/
1. Install CMake 3.19+, https://cmake.org/download/
1. Running the following commands in the root folder of the cloned repro:

```bash
> mkdir build
> cd build
> cmake ..
> cmake --build .
or
> cmake --build . --config Release
```

* You need to download **32bit version** of fftw library from fftw website http://www.fftw.org/install/windows.html. Copy libfftw3f-3.dll from the downloaded zip package to the same folder of extio DLL.

* If you are running **64bit** OS, you need to run the following different commands instead of "cmake .." based on your Visual Studio Version:
```
VS2019: >cmake .. -G "Visual Studio 16 2019" -A Win32
VS2017: >cmake .. -G "Visual Studio 15 2017 Win32"
VS2015: >cmake .. -G "Visual Studio 14 2015 Win32"
```

## Build Instructions for firmware

- download latest Cypress EZ-USB FX3 SDK from here: https://www.cypress.com/documentation/software-and-drivers/ez-usb-fx3-software-development-kit
- follow the installation instructions in the PDF document 'Getting Started with FX3 SDK'; on Windows the default installation path will be 'C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3' (see pages 17-18) - on Linux the installation path could be something like '/opt/Cypress/cyfx3sdk'
- add the following environment variables:
```
export FX3FWROOT=<installation path>
export ARMGCC_INSTALL_PATH=<ARM GCC installation path>
export ARMGCC_VERSION=4.8.1
```
(on Linux you may want to add those variables to your '.bash_profile' or '.profile')
- all the previous steps need to be done only once (or when you want to upgrade the version of the Cypress EZ-USB FX3 SDK)
- to compile the firmware run:
```
cd SDDC_FX3
make
```

## Directory structure:
    \Core\           > Core logic of the component
        r2iq.cpp			> The logic to demodulize IQ from ADC real samples
        FX3handler.cpp		> Interface with firmware
        RadioHandler.cpp    > The abstraction of different radios
        Radio\*.cpp         > Hardware specific logic
    \ExtIO_sddc\ 		> ExtIO_sddc sources,
        extio_sddc.cpp     > The implementation of EXTIO contract 
        tdialog.cpp			> The Configuration GUI Dialog
    \libsddc\        > libsddc lib
    \SDDC_FX3\          > Firmware sources

## Change Logs

### tag  v1.3.0RC1 Version "V1.2 RC1" date 4/11/2021
- Use Ringbuffer for input and output #157
- Delegate the VHF and HF decision to radio class #194
- Debug trace via USB #195
- Arm neon port #203
- A dialogBox with SDR name and FX3 serial number allows selection of a device from many. #210

 So far the known issues:
- Al power up sometime FX3 is not enumerated as Cypress USB FX3 BootLoader Device. When it happens also Cypress USB Control Center app is not able to detect it and a hardware disconnect and reconnect maybe required to enumerate the unit.

### tag  v1.2.0 Version "V1.2.0" date 18/3/2021
- Fix the crosstalk HF <-> VHF/UHF issue #177
- When HDSDR's version >= HDSDR v2.81 beta3 (March 8, 2021) following an ADC's sampling rate change the new IF bandwidths are computed dynamically and restart of HDSDR in not required.


### tag  v1.2RC1 Version "V1.2 RC1" date 18/2/2021
- The ADC's nominal sampling frequency is user selectable from 50 MHz to 140 MHz at 1Hz step, 
- The reference calibration can be adjusted in the dialog GUI in a range of +/- 200 ppm. (#171)
- The tuner IF center frequency is moved to 4.570 MHz that is the standard for RT820T. We were using 5 MHz before when we did not have yet fine tune ability. (#159)
- Support for AVX/2/512 instructions added. This change may reduce CPU usage for some modern hardware.(#152) 
- The Kaiser-Bessel IF filter with 85% of Nyquist band are computed at initialization. It simplifies managment of IF filters (#147)
- Add automatically build verification for both master branch and PRs. This  feature of the Github environment speeds development checks(#141)

 So far the known issues:
- The ADC sampling frequency can be selected via the ExtIO dialog.  HDSDR versions <= 2.80 require to close HDSDR.exe and restart the application to have the right IF sample rates. Higher HDSDR releases will have dynamically allocated IF sample rates and they will not require the restart.
- This release does not operate correctly with HF103 and similar designs that do not use a pll to generate the ADC sampling clock.
- The accuracy is that of the SI5351a programming about 1 Hz


### tag  v1.1.0 Version "V1.1.0" date 29/12/2020
- Fix the round of rf/if gains in the UI #109
- Fix sounds like clipping, on strong stations #120
- Fix reboot of FX3 #119

 So far the known issues:
- The reference frequency correction via HDSDR -> Options -> Calibration Settings ->LO Frequency Calibration doesn't work correctly. This problem will be addressed in the next release.

### tag  v1.1RC1 Version "V1.1 RC1" date 20/12/2020
- Supports 128M ADC sampling rate selectable via the dialog GUI ( Justin Peng and Howard Su )
- Use of libsddc allows development of Linux support and Soapy integration ( Franco Venturi https://github.com/fventuri/libsdd) 
- Tune the LO with a 1Hz step everywhere (Hayati Ayguen https://github.com/hayguen/pffft).
- Move multi thread r2iq to a multithreaded pipeline model to better leverage multi cores. Remove callback in USB (adcsample) thread to HDSDR to make sure we can reach 128Msps.
- Continuous tuning LW,MW,HF and VHF.
- Dialog GUI has samplerates and gains settings for use with other SDR applications than HDSDR.
- Test harmonic R820T tuning is there (Hayati Ayguen https://github.com/librtlsdr/librtlsdr/tree/development)
- the gain correction is made via  HDSDR -> Options -> Calibration Settings ->S-Meter Calibration.

 So far the known issues:
- The reference frequency correction via HDSDR -> Options -> Calibration Settings ->LO Frequency Calibration doesn't work correctly. This problem will be addressed in the next release.
- The 128M adc rate is experimental and must be activated manually in the ExtIO dialog GUI. It works with RX888 hardware that have 60 MHz LPF and requires a quite fast PC.

### tag  v1.01 Version "V1.01 RC1" date 06/11/2020
- SDDC_FX3 directory contains ARM sources and GPIFII project to compile sddc_fx3.img
- Detects the HW type: BBRF103, BBRF103, RX888 at runtime.
- Si5351a and R820T2 driver moved to FX3 code
- Redesign of FX3 control commands
- Rename of FX3handler (ex: OpenFX3) and RadioHandler (ex: BBRF103) modules
- Simplified ExtIO GUI Antenna BiasT, Dither, and Rand.
- Reference frequency correction via software +/- 200 ppm range
- Gain adjust +/-20 dB step 1dB
- R820T2 controls RF gains via a single control from GUI
- ExtIO.dll designed for HDSDR use.
- HF103 added a tuning limit at ADC_FREQ/2.

### tag  v0.98  Version "SDDC-0.98" date  13/06/2019
   R820T2 is enabled to support VHF

### tag  v0.96  Version "SDDC-0.96" date  25/02/2018

### tag  v0.95  Version "SDDC-0.95" date 31/08/2017

Initial version

## References
- EXTIO Specification from http://www.sdradio.eu/weaksignals/bin/Winrad_Extio.pdf
- Discussion and Support https://groups.io/g/NextGenSDRs
- Recommended Application http://www.weaksignals.com/
- http://www.hdsdr.de
- http://booyasdr.sourceforge.net/
- http://www.cypress.com/


## Many thanks to
- Alberto di Bene, I2PHD
- Mario Taeubel
- LightCoder (aka LC)
- Howard Su
- Hayati Ayguen
- Franco Venturi
- All the Others !

#### 2016,2017,2018,2019,2020,2021  IK1XPV Oscar Steila - ik1xpv(at)gmail.com
https://sdr-prototypes.blogspot.com/

http://www.steila.com/blog
