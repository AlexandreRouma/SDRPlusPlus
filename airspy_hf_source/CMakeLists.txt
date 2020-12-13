cmake_minimum_required(VERSION 3.13)
project(airspy_hf_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(airspy_hf_source SHARED ${SRC})
target_link_libraries(airspy_hf_source PRIVATE sdrpp_core)
set_target_properties(airspy_hf_source PROPERTIES PREFIX "")

if(WIN32)
  target_link_libraries(airspy_hf_source PRIVATE wsock32 ws2_32)
endif()