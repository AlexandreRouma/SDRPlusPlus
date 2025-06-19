cmake_minimum_required(VERSION 3.13)
project(sdrpp)

# Backends
option(OPT_BACKEND_GLFW "Use the GLFW backend" ON)
option(OPT_BACKEND_ANDROID "Use the Android backend" OFF)

# Compatibility Options
option(OPT_OVERRIDE_STD_FILESYSTEM "Use a local version of std::filesystem on systems that don't have it yet" OFF)

# Sources
option(OPT_BUILD_AIRSPY_SOURCE "Build Airspy Source Module (Dependencies: libairspy)" ON)
option(OPT_BUILD_AIRSPYHF_SOURCE "Build Airspy HF+ Source Module (Dependencies: libairspyhf)" ON)
option(OPT_BUILD_AUDIO_SOURCE "Build Audio Source Module (Dependencies: rtaudio)" ON)
option(OPT_BUILD_BADGESDR_SOURCE "Build BadgeSDR Source Module (Dependencies: libusb)" OFF)
option(OPT_BUILD_BLADERF_SOURCE "Build BladeRF Source Module (Dependencies: libbladeRF)" OFF)
option(OPT_BUILD_FILE_SOURCE "Wav file source" ON)
option(OPT_BUILD_FOBOSSDR_SOURCE "Build FobosSDR Source Module (Dependencies: libfobos)" OFF)
option(OPT_BUILD_HACKRF_SOURCE "Build HackRF Source Module (Dependencies: libhackrf)" ON)
option(OPT_BUILD_HAROGIC_SOURCE "Build Harogic Source Module (Dependencies: htra_api)" OFF)
option(OPT_BUILD_HERMES_SOURCE "Build Hermes Source Module (no dependencies required)" ON)
option(OPT_BUILD_HYDRASDR_SOURCE "Build HydraSDR Source Module (Dependencies: libhydrasdr)" OFF)
option(OPT_BUILD_KCSDR_SOURCE "Build KCSDR Source Module (Dependencies: libkcsdr)" OFF)
option(OPT_BUILD_LIMESDR_SOURCE "Build LimeSDR Source Module (Dependencies: liblimesuite)" OFF)
option(OPT_BUILD_NETWORK_SOURCE "Build Network Source Module (no dependencies required)" ON)
option(OPT_BUILD_PERSEUS_SOURCE "Build Perseus Source Module (Dependencies: libperseus-sdr)" OFF)
option(OPT_BUILD_PLUTOSDR_SOURCE "Build PlutoSDR Source Module (Dependencies: libiio, libad9361)" ON)
option(OPT_BUILD_RFNM_SOURCE "Build RFNM Source Module (Dependencies: librfnm)" OFF)
option(OPT_BUILD_RFSPACE_SOURCE "Build RFspace Source Module (no dependencies required)" ON)
option(OPT_BUILD_RTL_SDR_SOURCE "Build RTL-SDR Source Module (Dependencies: librtlsdr)" ON)
option(OPT_BUILD_RTL_TCP_SOURCE "Build RTL-TCP Source Module (no dependencies required)" ON)
option(OPT_BUILD_SDRPP_SERVER_SOURCE "Build SDR++ Server Source Module (no dependencies required)" ON)
option(OPT_BUILD_SDRPLAY_SOURCE "Build SDRplay Source Module (Dependencies: libsdrplay)" OFF)
option(OPT_BUILD_SOAPY_SOURCE "Build SoapySDR Source Module (Dependencies: soapysdr)" OFF)
option(OPT_BUILD_SPECTRAN_SOURCE "Build Spectran Source Module (Dependencies: Aaronia RTSA Suite)" OFF)
option(OPT_BUILD_SPECTRAN_HTTP_SOURCE "Build Spectran HTTP Source Module (no dependencies required)" ON)
option(OPT_BUILD_SPYSERVER_SOURCE "Build SpyServer Source Module (no dependencies required)" ON)
option(OPT_BUILD_USRP_SOURCE "Build USRP Source Module (libuhd)" OFF)

# Sinks
option(OPT_BUILD_ANDROID_AUDIO_SINK "Build Android Audio Sink Module (Dependencies: AAudio, only for android)" OFF)
option(OPT_BUILD_AUDIO_SINK "Build Audio Sink Module (Dependencies: rtaudio)" ON)
option(OPT_BUILD_NETWORK_SINK "Build Audio Sink Module (no dependencies required)" ON)
option(OPT_BUILD_NEW_PORTAUDIO_SINK "Build the new PortAudio Sink Module (Dependencies: portaudio)" OFF)
option(OPT_BUILD_PORTAUDIO_SINK "Build PortAudio Sink Module (Dependencies: portaudio)" OFF)

# Decoders
option(OPT_BUILD_ATV_DECODER "Build ATV decoder (no dependencies required)" OFF)
option(OPT_BUILD_DAB_DECODER "Build the DAB/DAB+ decoder (no dependencies required)" OFF)
option(OPT_BUILD_FALCON9_DECODER "Build the falcon9 live decoder (Dependencies: ffplay)" OFF)
option(OPT_BUILD_KG_SSTV_DECODER "Build the KG SSTV (KG-STV) decoder module (no dependencies required)" OFF)
option(OPT_BUILD_M17_DECODER "Build the M17 decoder module (Dependencies: codec2)" OFF)
option(OPT_BUILD_METEOR_DEMODULATOR "Build the meteor demodulator module (no dependencies required)" ON)
option(OPT_BUILD_PAGER_DECODER "Build the pager decoder module (no dependencies required)" ON)
option(OPT_BUILD_RADIO "Main audio modulation decoder (AM, FM, SSB, etc...)" ON)
option(OPT_BUILD_RYFI_DECODER "RyFi data link decoder" OFF)
option(OPT_BUILD_VOR_RECEIVER "VOR beacon receiver" OFF)
option(OPT_BUILD_WEATHER_SAT_DECODER "Build the HRPT decoder module (no dependencies required)" OFF)

# Misc
option(OPT_BUILD_DISCORD_PRESENCE "Build the Discord Rich Presence module" ON)
option(OPT_BUILD_FREQUENCY_MANAGER "Build the Frequency Manager module" ON)
option(OPT_BUILD_IQ_EXPORTER "Build the IQ Exporter module" ON)
option(OPT_BUILD_RECORDER "Audio and baseband recorder" ON)
option(OPT_BUILD_RIGCTL_CLIENT "Rigctl client to make SDR++ act as a panadapter" ON)
option(OPT_BUILD_RIGCTL_SERVER "Rigctl backend for controlling SDR++ with software like gpredict" ON)
option(OPT_BUILD_SCANNER "Frequency scanner" ON)
option(OPT_BUILD_SCHEDULER "Build the scheduler" OFF)

# Other options
option(USE_INTERNAL_LIBCORRECT "Use an internal version of libcorrect" ON)
option(USE_BUNDLE_DEFAULTS "Set the default resource and module directories to the right ones for a MacOS .app" OFF)
option(COPY_MSVC_REDISTRIBUTABLES "Copy over the Visual C++ Redistributable" OFF)

# Module cmake path
set(SDRPP_MODULE_CMAKE "${CMAKE_SOURCE_DIR}/sdrpp_module.cmake")

# Root source folder
set(SDRPP_CORE_ROOT "${CMAKE_SOURCE_DIR}/core/src/")

# Compiler flags
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    # Debug Flags
    if (MSVC)
        set(SDRPP_COMPILER_FLAGS /std:c++17 /EHsc)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17)
    endif ()
else()
    # Normal Flags
    if (MSVC)
        set(SDRPP_COMPILER_FLAGS /O2 /Ob2 /std:c++17 /EHsc)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17)
    endif ()
endif()
set(SDRPP_MODULE_COMPILER_FLAGS ${SDRPP_COMPILER_FLAGS})

# Set a default install prefix
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "..." FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "..." FORCE)
    endif()
endif()

# Configure toolchain for android
if (ANDROID)
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate"
    )
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=c++17")
endif (ANDROID)

# Core of SDR++
add_subdirectory("core")

# Source modules
if (OPT_BUILD_AIRSPY_SOURCE)
add_subdirectory("source_modules/airspy_source")
endif (OPT_BUILD_AIRSPY_SOURCE)

if (OPT_BUILD_AIRSPYHF_SOURCE)
add_subdirectory("source_modules/airspyhf_source")
endif (OPT_BUILD_AIRSPYHF_SOURCE)

if (OPT_BUILD_AUDIO_SOURCE)
add_subdirectory("source_modules/audio_source")
endif (OPT_BUILD_AUDIO_SOURCE)

if (OPT_BUILD_BADGESDR_SOURCE)
add_subdirectory("source_modules/badgesdr_source")
endif (OPT_BUILD_BADGESDR_SOURCE)

if (OPT_BUILD_BLADERF_SOURCE)
add_subdirectory("source_modules/bladerf_source")
endif (OPT_BUILD_BLADERF_SOURCE)

if (OPT_BUILD_FILE_SOURCE)
add_subdirectory("source_modules/file_source")
endif (OPT_BUILD_FILE_SOURCE)

if (OPT_BUILD_FOBOSSDR_SOURCE)
add_subdirectory("source_modules/fobossdr_source")
endif (OPT_BUILD_FOBOSSDR_SOURCE)

if (OPT_BUILD_HACKRF_SOURCE)
add_subdirectory("source_modules/hackrf_source")
endif (OPT_BUILD_HACKRF_SOURCE)

if (OPT_BUILD_HAROGIC_SOURCE)
add_subdirectory("source_modules/harogic_source")
endif (OPT_BUILD_HAROGIC_SOURCE)

if (OPT_BUILD_HERMES_SOURCE)
add_subdirectory("source_modules/hermes_source")
endif (OPT_BUILD_HERMES_SOURCE)

if (OPT_BUILD_HYDRASDR_SOURCE)
add_subdirectory("source_modules/hydrasdr_source")
endif (OPT_BUILD_HYDRASDR_SOURCE)

if (OPT_BUILD_KCSDR_SOURCE)
add_subdirectory("source_modules/kcsdr_source")
endif (OPT_BUILD_KCSDR_SOURCE)

if (OPT_BUILD_LIMESDR_SOURCE)
add_subdirectory("source_modules/limesdr_source")
endif (OPT_BUILD_LIMESDR_SOURCE)

if (OPT_BUILD_NETWORK_SOURCE)
add_subdirectory("source_modules/network_source")
endif (OPT_BUILD_NETWORK_SOURCE)

if (OPT_BUILD_PERSEUS_SOURCE)
add_subdirectory("source_modules/perseus_source")
endif (OPT_BUILD_PERSEUS_SOURCE)

if (OPT_BUILD_PLUTOSDR_SOURCE)
add_subdirectory("source_modules/plutosdr_source")
endif (OPT_BUILD_PLUTOSDR_SOURCE)

if (OPT_BUILD_RFNM_SOURCE)
add_subdirectory("source_modules/rfnm_source")
endif (OPT_BUILD_RFNM_SOURCE)

if (OPT_BUILD_RFSPACE_SOURCE)
add_subdirectory("source_modules/rfspace_source")
endif (OPT_BUILD_RFSPACE_SOURCE)

if (OPT_BUILD_RTL_SDR_SOURCE)
add_subdirectory("source_modules/rtl_sdr_source")
endif (OPT_BUILD_RTL_SDR_SOURCE)

if (OPT_BUILD_RTL_TCP_SOURCE)
add_subdirectory("source_modules/rtl_tcp_source")
endif (OPT_BUILD_RTL_TCP_SOURCE)

if (OPT_BUILD_SDDC_SOURCE)
add_subdirectory("source_modules/sddc_source")
endif (OPT_BUILD_SDDC_SOURCE)

if (OPT_BUILD_SDRPP_SERVER_SOURCE)
add_subdirectory("source_modules/sdrpp_server_source")
endif (OPT_BUILD_SDRPP_SERVER_SOURCE)

if (OPT_BUILD_SDRPLAY_SOURCE)
add_subdirectory("source_modules/sdrplay_source")
endif (OPT_BUILD_SDRPLAY_SOURCE)

if (OPT_BUILD_SOAPY_SOURCE)
add_subdirectory("source_modules/soapy_source")
endif (OPT_BUILD_SOAPY_SOURCE)

if (OPT_BUILD_SPECTRAN_SOURCE)
add_subdirectory("source_modules/spectran_source")
endif (OPT_BUILD_SPECTRAN_SOURCE)

if (OPT_BUILD_SPECTRAN_HTTP_SOURCE)
add_subdirectory("source_modules/spectran_http_source")
endif (OPT_BUILD_SPECTRAN_HTTP_SOURCE)

if (OPT_BUILD_SPYSERVER_SOURCE)
add_subdirectory("source_modules/spyserver_source")
endif (OPT_BUILD_SPYSERVER_SOURCE)

if (OPT_BUILD_USRP_SOURCE)
add_subdirectory("source_modules/usrp_source")
endif (OPT_BUILD_USRP_SOURCE)


# Sink modules
if (OPT_BUILD_ANDROID_AUDIO_SINK)
add_subdirectory("sink_modules/android_audio_sink")
endif (OPT_BUILD_ANDROID_AUDIO_SINK)

if (OPT_BUILD_AUDIO_SINK)
add_subdirectory("sink_modules/audio_sink")
endif (OPT_BUILD_AUDIO_SINK)

if (OPT_BUILD_PORTAUDIO_SINK)
add_subdirectory("sink_modules/portaudio_sink")
endif (OPT_BUILD_PORTAUDIO_SINK)

if (OPT_BUILD_NETWORK_SINK)
add_subdirectory("sink_modules/network_sink")
endif (OPT_BUILD_NETWORK_SINK)

if (OPT_BUILD_NEW_PORTAUDIO_SINK)
add_subdirectory("sink_modules/new_portaudio_sink")
endif (OPT_BUILD_NEW_PORTAUDIO_SINK)


# Decoders
if (OPT_BUILD_ATV_DECODER)
add_subdirectory("decoder_modules/atv_decoder")
endif (OPT_BUILD_ATV_DECODER)

if (OPT_BUILD_DAB_DECODER)
add_subdirectory("decoder_modules/dab_decoder")
endif (OPT_BUILD_DAB_DECODER)

if (OPT_BUILD_FALCON9_DECODER)
add_subdirectory("decoder_modules/falcon9_decoder")
endif (OPT_BUILD_FALCON9_DECODER)

if (OPT_BUILD_KG_SSTV_DECODER)
add_subdirectory("decoder_modules/kg_sstv_decoder")
endif (OPT_BUILD_KG_SSTV_DECODER)

if (OPT_BUILD_M17_DECODER)
add_subdirectory("decoder_modules/m17_decoder")
endif (OPT_BUILD_M17_DECODER)

if (OPT_BUILD_METEOR_DEMODULATOR)
add_subdirectory("decoder_modules/meteor_demodulator")
endif (OPT_BUILD_METEOR_DEMODULATOR)

if (OPT_BUILD_PAGER_DECODER)
add_subdirectory("decoder_modules/pager_decoder")
endif (OPT_BUILD_PAGER_DECODER)

if (OPT_BUILD_RADIO)
add_subdirectory("decoder_modules/radio")
endif (OPT_BUILD_RADIO)

if (OPT_BUILD_RYFI_DECODER)
add_subdirectory("decoder_modules/ryfi_decoder")
endif (OPT_BUILD_RYFI_DECODER)

if (OPT_BUILD_VOR_RECEIVER)
add_subdirectory("decoder_modules/vor_receiver")
endif (OPT_BUILD_VOR_RECEIVER)

if (OPT_BUILD_WEATHER_SAT_DECODER)
add_subdirectory("decoder_modules/weather_sat_decoder")
endif (OPT_BUILD_WEATHER_SAT_DECODER)


# Misc
if (OPT_BUILD_DISCORD_PRESENCE)
add_subdirectory("misc_modules/discord_integration")
endif (OPT_BUILD_DISCORD_PRESENCE)

if (OPT_BUILD_FREQUENCY_MANAGER)
add_subdirectory("misc_modules/frequency_manager")
endif (OPT_BUILD_FREQUENCY_MANAGER)

if (OPT_BUILD_IQ_EXPORTER)
add_subdirectory("misc_modules/iq_exporter")
endif (OPT_BUILD_IQ_EXPORTER)

if (OPT_BUILD_RECORDER)
add_subdirectory("misc_modules/recorder")
endif (OPT_BUILD_RECORDER)

if (OPT_BUILD_RIGCTL_CLIENT)
add_subdirectory("misc_modules/rigctl_client")
endif (OPT_BUILD_RIGCTL_CLIENT)

if (OPT_BUILD_RIGCTL_SERVER)
add_subdirectory("misc_modules/rigctl_server")
endif (OPT_BUILD_RIGCTL_SERVER)

if (OPT_BUILD_SCANNER)
add_subdirectory("misc_modules/scanner")
endif (OPT_BUILD_SCANNER)

if (OPT_BUILD_SCHEDULER)
add_subdirectory("misc_modules/scheduler")
endif (OPT_BUILD_SCHEDULER)

if (MSVC)
    add_executable(sdrpp "src/main.cpp" "win32/resources.rc")
else ()
    add_executable(sdrpp "src/main.cpp")
endif ()

target_link_libraries(sdrpp PRIVATE sdrpp_core)

# Compiler arguments
target_compile_options(sdrpp PRIVATE ${SDRPP_COMPILER_FLAGS})

# Copy dynamic libs over
if (MSVC)
    add_custom_target(do_always ALL xcopy /s \"$<TARGET_FILE_DIR:sdrpp_core>\\*.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)
    add_custom_target(do_always_volk ALL xcopy /s \"C:/Program Files/PothosSDR/bin\\volk.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)

    if (COPY_MSVC_REDISTRIBUTABLES)
        # Get the list of Visual C++ runtime DLLs
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP True)
        include(InstallRequiredSystemLibraries)

        # Create a space sperated list
        set(REDIST_DLLS_STR "")
        foreach(DLL IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
            set(REDIST_DLLS_STR COMMAND xcopy /F \"${DLL}\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y ${REDIST_DLLS_STR})
        endforeach()
        
        # Create target
        add_custom_target(do_always_msvc ALL ${REDIST_DLLS_STR})
    endif ()
endif ()


if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    target_link_libraries(sdrpp PUBLIC pthread)
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.dylib\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" -DOPT_BUILD_BLADERF_SOURCE=ON -DOPT_BUILD_LIMESDR_SOURCE=ON -DOPT_BUILD_SDRPLAY_SOURCE=ON -DOPT_BUILD_M17_DECODER=ON -DOPT_BUILD_SCANNER=ON -DOPT_BUILD_SCHEDULER=ON -DOPT_BUILD_USRP_SOURCE=ON -DOPT_BUILD_PAGER_DECODER=ON

# Create module cmake file
configure_file(${CMAKE_SOURCE_DIR}/sdrpp_module.cmake ${CMAKE_CURRENT_BINARY_DIR}/sdrpp_module.cmake @ONLY)

# Install directives
install(TARGETS sdrpp DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/bandplans DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/colormaps DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/fonts DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/icons DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/themes DESTINATION share/sdrpp)
configure_file(${CMAKE_SOURCE_DIR}/sdrpp.desktop ${CMAKE_CURRENT_BINARY_DIR}/sdrpp.desktop @ONLY)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sdrpp.desktop DESTINATION share/applications)
endif ()

# Create uninstall target
configure_file(${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake @ONLY)
add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Create headers target