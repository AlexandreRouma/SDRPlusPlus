cmake_minimum_required(VERSION 3.13)
project(rfnm_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(rfnm_source PRIVATE "C:/Users/ryzerth/Documents/Code/librfnm/include/librfnm")
target_link_directories(rfnm_source PRIVATE "C:/Users/ryzerth/Documents/Code/librfnm/build/Release")
target_link_libraries(rfnm_source PRIVATE librfnm)