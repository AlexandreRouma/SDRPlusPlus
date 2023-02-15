cmake_minimum_required(VERSION 3.13)
project(airspy_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(airspy_source PRIVATE "C:/Program Files/PothosSDR/bin/")

    target_include_directories(airspy_source PUBLIC "C:/Program Files/PothosSDR/include/libairspy/")

    target_link_libraries(airspy_source PRIVATE airspy)
elseif (ANDROID)
    target_include_directories(airspy_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/include/libairspy
    )

    target_link_libraries(airspy_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/lib/libusb1.0.so
        /sdr-kit/${ANDROID_ABI}/lib/libairspy.so
    )
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBAIRSPY REQUIRED libairspy)

    target_include_directories(airspy_source PRIVATE ${LIBAIRSPY_INCLUDE_DIRS})
    target_link_directories(airspy_source PRIVATE ${LIBAIRSPY_LIBRARY_DIRS})
    target_link_libraries(airspy_source PRIVATE ${LIBAIRSPY_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(airspy_source PRIVATE "/usr/local/include")
    endif()

endif ()