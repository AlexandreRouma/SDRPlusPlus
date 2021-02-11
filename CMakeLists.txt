cmake_minimum_required(VERSION 3.13)
project(sdrpp)

# Default to /usr for CMAKE_INSTALL_PREFIX
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Default install prefix" FORCE)
endif()

option(OPT_BUILD_RTL_TCP_SOURCE "Build RTL-TCP Source Module (no dependencies required)" ON)
option(OPT_BUILD_SPYSERVER_SOURCE "Build SpyServer Source Module (no dependencies required)" ON)
option(OPT_BUILD_SOAPY_SOURCE "Build SoapySDR Source Module (Depedencies: soapysdr)" ON)
option(OPT_BUILD_AIRSPYHF_SOURCE "Build Airspy HF+ Source Module (Depedencies: libairspyhf)" ON)
option(OPT_BUILD_AIRSPY_SOURCE "Build Airspy Source Module (Depedencies: libairspy)" ON)
option(OPT_BUILD_SDRPLAY_SOURCE "Build SDRplay Source Module (Depedencies: libsdrplay)" ON)
option(OPT_BUILD_PLUTOSDR_SOURCE "Build PlutoSDR Source Module (Depedencies: libiio, libad9361)" ON)
option(OPT_BUILD_HACKRF_SOURCE "Build HackRF Source Module (Depedencies: libhackrf)" ON)
option(OPT_BUILD_AUDIO_SINK "Build Audio Sink Module (Depedencies: portaudio)" ON)

# Core of SDR++
add_subdirectory("core")

# Base modules
add_subdirectory("radio")
add_subdirectory("recorder") 
add_subdirectory("file_source")

# Source modules
if (OPT_BUILD_RTL_TCP_SOURCE)
add_subdirectory("rtl_tcp_source")
endif (OPT_BUILD_RTL_TCP_SOURCE)

if (OPT_BUILD_SPYSERVER_SOURCE)
add_subdirectory("spyserver_source")
endif (OPT_BUILD_SPYSERVER_SOURCE)

if (OPT_BUILD_SOAPY_SOURCE)
add_subdirectory("soapy_source")
endif (OPT_BUILD_SOAPY_SOURCE)

if (OPT_BUILD_AIRSPYHF_SOURCE)
add_subdirectory("airspyhf_source")
endif (OPT_BUILD_AIRSPYHF_SOURCE)

if (OPT_BUILD_AIRSPY_SOURCE)
add_subdirectory("airspy_source")
endif (OPT_BUILD_AIRSPY_SOURCE)

if (OPT_BUILD_SDRPLAY_SOURCE)
add_subdirectory("sdrplay_source")
endif (OPT_BUILD_SDRPLAY_SOURCE)

if (OPT_BUILD_PLUTOSDR_SOURCE)
add_subdirectory("plutosdr_source")
endif (OPT_BUILD_PLUTOSDR_SOURCE)

if (OPT_BUILD_HACKRF_SOURCE)
add_subdirectory("hackrf_source")
endif (OPT_BUILD_HACKRF_SOURCE)

if (OPT_BUILD_AUDIO_SINK)
add_subdirectory("audio_sink")
endif (OPT_BUILD_AUDIO_SINK)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif (MSVC)

add_executable(sdrpp "src/main.cpp" "win32/resources.rc")
target_link_libraries(sdrpp PRIVATE sdrpp_core)

# Copy dynamic libs over
if (MSVC)
    add_custom_target(do_always ALL xcopy /s \"$<TARGET_FILE_DIR:sdrpp_core>\\*.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)
    add_custom_target(do_always_volk ALL xcopy /s \"C:/Program Files/PothosSDR/bin\\volk.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)
endif (MSVC)


if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.dylib\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"

# Install directives
install(TARGETS sdrpp DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/bandplans DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/colormaps DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/fonts DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/icons DESTINATION share/sdrpp)

# Create uninstall target
configure_file(${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake @ONLY)
add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)