cmake_minimum_required(VERSION 3.13)
project(rfnm_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(rfnm_source PRIVATE "C:/Program Files/RFNM/include")
target_link_directories(rfnm_source PRIVATE "C:/Program Files/RFNM/lib")
target_link_libraries(rfnm_source PRIVATE rfnm)