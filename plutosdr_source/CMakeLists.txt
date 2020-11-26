cmake_minimum_required(VERSION 3.13)
project(plutosdr_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(plutosdr_source SHARED ${SRC})
target_link_libraries(plutosdr_source PRIVATE sdrpp_core)
set_target_properties(plutosdr_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")

    target_link_libraries(plutosdr_source PUBLIC libiio)
    target_link_libraries(plutosdr_source PUBLIC libad9361)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(SOAPY REQUIRED iio)

    target_include_directories(plutosdr_source PUBLIC ${IIO_INCLUDE_DIRS})
    target_link_directories(plutosdr_source PUBLIC ${IIO_LIBRARY_DIRS})
    target_link_libraries(plutosdr_source PUBLIC ${IIO_LIBRARIES})
endif (MSVC)