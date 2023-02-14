cmake_minimum_required(VERSION 3.13)
project(frequency_manager)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(frequency_manager PRIVATE "src/" "../../decoder_modules/radio/src")