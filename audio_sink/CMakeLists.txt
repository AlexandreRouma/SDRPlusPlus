cmake_minimum_required(VERSION 3.13)
project(audio_sink)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(audio_sink SHARED ${SRC})
target_link_libraries(audio_sink PRIVATE sdrpp_core)
set_target_properties(audio_sink PROPERTIES PREFIX "")