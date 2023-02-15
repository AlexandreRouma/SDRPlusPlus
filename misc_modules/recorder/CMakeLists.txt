cmake_minimum_required(VERSION 3.13)
project(recorder)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(recorder PRIVATE "src/")