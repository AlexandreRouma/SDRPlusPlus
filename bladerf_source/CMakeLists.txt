cmake_minimum_required(VERSION 3.13)
project(bladerf_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(bladerf_source SHARED ${SRC})
target_link_libraries(bladerf_source PRIVATE sdrpp_core)
set_target_properties(bladerf_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(bladerf_source PUBLIC bladeRF)
else (MSVC)
    # Not in pkg-config
    target_link_libraries(bladerf_source PUBLIC libbladeRF)
endif ()

# Install directives
install(TARGETS bladerf_source DESTINATION lib/sdrpp/plugins)