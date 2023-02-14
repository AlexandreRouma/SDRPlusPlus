cmake_minimum_required(VERSION 3.13)
project(bladerf_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(bladerf_source PRIVATE "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(bladerf_source PRIVATE bladeRF)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBBLADERF REQUIRED libbladeRF)

    target_include_directories(bladerf_source PRIVATE ${LIBBLADERF_INCLUDE_DIRS})
    target_link_directories(bladerf_source PRIVATE ${LIBBLADERF_LIBRARY_DIRS})
    target_link_libraries(bladerf_source PRIVATE ${LIBBLADERF_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(bladerf_source PRIVATE "/usr/local/include")
    endif()
endif ()