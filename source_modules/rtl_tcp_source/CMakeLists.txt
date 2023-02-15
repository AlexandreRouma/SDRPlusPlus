cmake_minimum_required(VERSION 3.13)
project(rtl_tcp_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(rtl_tcp_source PRIVATE "src/")