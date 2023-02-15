cmake_minimum_required(VERSION 3.13)
project(spyserver_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(spyserver_source PRIVATE "src/")