cmake_minimum_required(VERSION 3.13)
project(weather_sat_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(weather_sat_decoder PRIVATE "src/")