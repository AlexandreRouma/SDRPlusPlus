cmake_minimum_required(VERSION 3.13)
project(demo)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive -fsanitize=address -g")
    # set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

file(GLOB SRC "src/*.cpp")

add_library(demo SHARED ${SRC})
target_link_libraries(demo PRIVATE sdrpp_core)
set_target_properties(demo PROPERTIES PREFIX "")