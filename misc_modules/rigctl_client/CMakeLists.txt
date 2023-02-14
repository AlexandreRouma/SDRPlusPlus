cmake_minimum_required(VERSION 3.13)
project(rigctl_client)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(rigctl_client PRIVATE "src/")
target_include_directories(rigctl_client PRIVATE "../recorder/src")
target_include_directories(rigctl_client PRIVATE "../../decoder_modules/meteor_demodulator/src")
target_include_directories(rigctl_client PRIVATE "../../decoder_modules/radio/src")