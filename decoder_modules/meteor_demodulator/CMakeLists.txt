cmake_minimum_required(VERSION 3.13)
project(meteor_demodulator)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(meteor_demodulator PRIVATE "src/")