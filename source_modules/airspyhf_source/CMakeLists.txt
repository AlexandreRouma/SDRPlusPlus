cmake_minimum_required(VERSION 3.13)
project(airspyhf_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(airspyhf_source PRIVATE "C:/Program Files/PothosSDR/bin/")

    target_include_directories(airspyhf_source PUBLIC "C:/Program Files/PothosSDR/include/libairspyhf/")

    target_link_libraries(airspyhf_source PRIVATE airspyhf)
elseif (ANDROID)
    target_include_directories(airspyhf_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/include/libairspyhf
    )

    target_link_libraries(airspyhf_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/lib/libusb1.0.so
        /sdr-kit/${ANDROID_ABI}/lib/libairspyhf.so
    )
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBAIRSPYHF REQUIRED libairspyhf)

    target_include_directories(airspyhf_source PRIVATE ${LIBAIRSPYHF_INCLUDE_DIRS})
    target_link_directories(airspyhf_source PRIVATE ${LIBAIRSPYHF_LIBRARY_DIRS})
    target_link_libraries(airspyhf_source PRIVATE ${LIBAIRSPYHF_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(airspyhf_source PRIVATE "/usr/local/include")
    endif()
    
endif ()