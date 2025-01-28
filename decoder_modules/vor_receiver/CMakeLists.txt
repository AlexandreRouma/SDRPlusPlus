cmake_minimum_required(VERSION 3.13)
project(vor_receiver)

file(GLOB_RECURSE SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(vor_receiver PRIVATE "src/")