# Sources
option(OPT_BUILD_AIRSPY_SOURCE "Build Airspy Source Module (Depedencies: libairspy)" ON)
option(OPT_BUILD_AIRSPYHF_SOURCE "Build Airspy HF+ Source Module (Depedencies: libairspyhf)" ON)
option(OPT_BUILD_BLADERF_SOURCE "Build BladeRF Source Module (Depedencies: libbladeRF)" OFF)
option(OPT_BUILD_FILE_SOURCE "Wav file source" ON)
option(OPT_BUILD_HACKRF_SOURCE "Build HackRF Source Module (Depedencies: libhackrf)" ON)
option(OPT_BUILD_LIMESDR_SOURCE "Build LimeSDR Source Module (Depedencies: liblimesuite)" OFF)
option(OPT_BUILD_SDDC_SOURCE "Build SDDC Source Module (Depedencies: libusb-1.0)" OFF)
option(OPT_BUILD_RTL_SDR_SOURCE "Build RTL-SDR Source Module (Depedencies: librtlsdr)" ON)
option(OPT_BUILD_RTL_TCP_SOURCE "Build RTL-TCP Source Module (no dependencies required)" ON)
option(OPT_BUILD_SDRPLAY_SOURCE "Build SDRplay Source Module (Depedencies: libsdrplay)" OFF)
option(OPT_BUILD_SOAPY_SOURCE "Build SoapySDR Source Module (Depedencies: soapysdr)" ON)
option(OPT_BUILD_SPYSERVER_SOURCE "Build SpyServer Source Module (no dependencies required)" ON)
option(OPT_BUILD_PLUTOSDR_SOURCE "Build PlutoSDR Source Module (Depedencies: libiio, libad9361)" ON)

# Sinks
option(OPT_BUILD_AUDIO_SINK "Build Audio Sink Module (Depedencies: rtaudio)" ON)
option(OPT_BUILD_PORTAUDIO_SINK "Build PortAudio Sink Module (Depedencies: portaudio)" OFF)
option(OPT_BUILD_NETWORK_SINK "Build Audio Sink Module (no dependencies required)" ON)
option(OPT_BUILD_NEW_PORTAUDIO_SINK "Build the new PortAudio Sink Module (Depedencies: portaudio)" OFF)

# Decoders
option(OPT_BUILD_FALCON9_DECODER "Build the falcon9 live decoder (Dependencies: ffplay)" OFF)
option(OPT_BUILD_METEOR_DEMODULATOR "Build the meteor demodulator module (no dependencies required)" ON)
option(OPT_BUILD_RADIO "Main audio modulation decoder (AM, FM, SSB, etc...)" ON)
option(OPT_BUILD_WEATHER_SAT_DECODER "Build the HRPT decoder module (no dependencies required)" ON)

# Misc
option(OPT_BUILD_DISCORD_PRESENCE "Build the Discord Rich Presence module" ON)
option(OPT_BUILD_FREQUENCY_MANAGER "Build the Frequency Manager module" ON)
option(OPT_BUILD_RECORDER "Audio and baseband recorder" ON)
option(OPT_BUILD_RIGCTL_SERVER "Rigctl backend for controlling SDR++ with software like gpredict" ON)

# Source modules
if (OPT_BUILD_AIRSPY_SOURCE)
add_subdirectory("airspy_source")
endif (OPT_BUILD_AIRSPY_SOURCE)

if (OPT_BUILD_AIRSPYHF_SOURCE)
add_subdirectory("airspyhf_source")
endif (OPT_BUILD_AIRSPYHF_SOURCE)

if (OPT_BUILD_BLADERF_SOURCE)
add_subdirectory("bladerf_source")
endif (OPT_BUILD_BLADERF_SOURCE)

if (OPT_BUILD_FILE_SOURCE)
add_subdirectory("file_source")
endif (OPT_BUILD_FILE_SOURCE)

if (OPT_BUILD_HACKRF_SOURCE)
add_subdirectory("hackrf_source")
endif (OPT_BUILD_HACKRF_SOURCE)

if (OPT_BUILD_LIMESDR_SOURCE)
add_subdirectory("limesdr_source")
endif (OPT_BUILD_LIMESDR_SOURCE)

if (OPT_BUILD_SDDC_SOURCE)
add_subdirectory("sddc_source")
endif (OPT_BUILD_SDDC_SOURCE)

if (OPT_BUILD_RTL_SDR_SOURCE)
add_subdirectory("rtl_sdr_source")
endif (OPT_BUILD_RTL_SDR_SOURCE)

if (OPT_BUILD_RTL_TCP_SOURCE)
add_subdirectory("rtl_tcp_source")
endif (OPT_BUILD_RTL_TCP_SOURCE)

if (OPT_BUILD_SDRPLAY_SOURCE)
add_subdirectory("sdrplay_source")
endif (OPT_BUILD_SDRPLAY_SOURCE)

if (OPT_BUILD_SOAPY_SOURCE)
add_subdirectory("soapy_source")
endif (OPT_BUILD_SOAPY_SOURCE)

if (OPT_BUILD_SPYSERVER_SOURCE)
add_subdirectory("spyserver_source")
endif (OPT_BUILD_SPYSERVER_SOURCE)

if (OPT_BUILD_PLUTOSDR_SOURCE)
add_subdirectory("plutosdr_source")
endif (OPT_BUILD_PLUTOSDR_SOURCE)


# Sink modules
if (OPT_BUILD_AUDIO_SINK)
add_subdirectory("audio_sink")
endif (OPT_BUILD_AUDIO_SINK)

if (OPT_BUILD_PORTAUDIO_SINK)
add_subdirectory("portaudio_sink")
endif (OPT_BUILD_PORTAUDIO_SINK)

if (OPT_BUILD_NETWORK_SINK)
add_subdirectory("network_sink")
endif (OPT_BUILD_NETWORK_SINK)

if (OPT_BUILD_NEW_PORTAUDIO_SINK)
add_subdirectory("new_portaudio_sink")
endif (OPT_BUILD_NEW_PORTAUDIO_SINK)


# Decoders
if (OPT_BUILD_FALCON9_DECODER)
add_subdirectory("falcon9_decoder")
endif (OPT_BUILD_FALCON9_DECODER)

if (OPT_BUILD_METEOR_DEMODULATOR)
add_subdirectory("meteor_demodulator")
endif (OPT_BUILD_METEOR_DEMODULATOR)

if (OPT_BUILD_RADIO)
add_subdirectory("radio")
endif (OPT_BUILD_RADIO)

if (OPT_BUILD_WEATHER_SAT_DECODER)
add_subdirectory("weather_sat_decoder")
endif (OPT_BUILD_WEATHER_SAT_DECODER)


# Misc
if (OPT_BUILD_DISCORD_PRESENCE)
add_subdirectory("discord_integration")
endif (OPT_BUILD_DISCORD_PRESENCE)

if (OPT_BUILD_FREQUENCY_MANAGER)
add_subdirectory("frequency_manager")
endif (OPT_BUILD_FREQUENCY_MANAGER)

if (OPT_BUILD_RECORDER)
add_subdirectory("recorder")
endif (OPT_BUILD_RECORDER)

if (OPT_BUILD_RIGCTL_SERVER)
add_subdirectory("rigctl_server")
endif (OPT_BUILD_RIGCTL_SERVER)
