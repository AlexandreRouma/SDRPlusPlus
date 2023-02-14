cmake_minimum_required(VERSION 3.13)
project(falcon9_decoder)

file(GLOB_RECURSE SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(falcon9_decoder PRIVATE "src/")