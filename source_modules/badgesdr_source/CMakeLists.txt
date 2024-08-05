cmake_minimum_required(VERSION 3.13)
project(badgesdr_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    find_package(libusb CONFIG REQUIRED)
    target_include_directories(badgesdr_source PRIVATE ${LIBUSB_INCLUDE_DIRS})
    target_link_libraries(badgesdr_source PRIVATE ${LIBUSB_LIBRARIES})
elseif (ANDROID)
    target_link_libraries(badgesdr_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/lib/libusb1.0.so
        /sdr-kit/${ANDROID_ABI}/lib/librtlsdr.so
    )
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

    target_include_directories(badgesdr_source PRIVATE ${LIBUSB_INCLUDE_DIRS})
    target_link_directories(badgesdr_source PRIVATE ${LIBUSB_LIBRARY_DIRS})
    target_link_libraries(badgesdr_source PRIVATE ${LIBUSB_LIBRARIES})
endif ()