cmake_minimum_required(VERSION 3.13)
project(limesdr_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(limesdr_source PRIVATE "C:/Program Files/PothosSDR/lib")

    target_link_libraries(limesdr_source PRIVATE LimeSuite)
else (MSVC)
    find_package(PkgConfig)

    # Not in pkg-config
    target_link_libraries(limesdr_source PRIVATE LimeSuite)

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(limesdr_source PRIVATE "/usr/local/include")
    endif()
endif ()