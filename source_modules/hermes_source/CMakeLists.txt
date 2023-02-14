cmake_minimum_required(VERSION 3.13)
project(hermes_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(hermes_source PRIVATE "src/")