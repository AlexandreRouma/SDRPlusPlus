cmake_minimum_required(VERSION 3.13)
project(rfspace_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(rfspace_source PRIVATE "src/")