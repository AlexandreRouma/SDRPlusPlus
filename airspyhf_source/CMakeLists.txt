cmake_minimum_required(VERSION 3.13)
project(airspyhf_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(airspyhf_source SHARED ${SRC})
target_link_libraries(airspyhf_source PRIVATE sdrpp_core)
set_target_properties(airspyhf_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(airspyhf_source PUBLIC airspyhf)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(SOAPY REQUIRED libairspyhf)

    target_include_directories(airspyhf_source PUBLIC ${LIBAIRSPYHF_INCLUDE_DIRS})
    target_link_directories(airspyhf_source PUBLIC ${AIRSPYHF_LIBRARY_DIRS})
    target_link_libraries(airspyhf_source PUBLIC ${LIBAIRSPYHF_LIBRARIES})
endif (MSVC)