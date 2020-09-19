cmake_minimum_required(VERSION 3.13)
project(demo)

file(GLOB SRC "src/*.cpp")

add_library(demo SHARED ${SRC})
target_link_libraries(demo PRIVATE sdrpp_core)