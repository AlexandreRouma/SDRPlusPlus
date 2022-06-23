cmake_minimum_required(VERSION 3.13)
project(hackrf_source)

file(GLOB SRC "src/*.cpp")

add_library(hackrf_source SHARED ${SRC})
target_link_libraries(hackrf_source PRIVATE sdrpp_core)
set_target_properties(hackrf_source PROPERTIES PREFIX "")

target_include_directories(hackrf_source PRIVATE "src/")

if (MSVC)
    # Lib path
    target_link_directories(hackrf_source PRIVATE "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(hackrf_source PRIVATE hackrf)
elseif (ANDROID)
    target_include_directories(hackrf_source PUBLIC
        /mnt/android_sdr/libusb/libusb
        /mnt/android_sdr/hackrf/host/libhackrf/src
    )

    target_link_libraries(hackrf_source PUBLIC
        /mnt/android_sdr/output/libusb/${ANDROID_ABI}/libusb1.0.so
        /mnt/android_sdr/output/libhackrf/${ANDROID_ABI}/libhackrf.so
    )
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBHACKRF REQUIRED libhackrf)

    target_include_directories(hackrf_source PRIVATE ${LIBHACKRF_INCLUDE_DIRS})
    target_link_directories(hackrf_source PRIVATE ${LIBHACKRF_LIBRARY_DIRS})
    target_link_libraries(hackrf_source PRIVATE ${LIBHACKRF_LIBRARIES})
endif ()

# Install directives
install(TARGETS hackrf_source DESTINATION lib/sdrpp/plugins)