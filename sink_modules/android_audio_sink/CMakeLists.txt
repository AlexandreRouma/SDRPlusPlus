cmake_minimum_required(VERSION 3.13)
project(audio_sink)

file(GLOB SRC "src/*.cpp")

add_library(audio_sink SHARED ${SRC})
target_link_libraries(audio_sink PRIVATE sdrpp_core)
set_target_properties(audio_sink PROPERTIES PREFIX "")

target_include_directories(audio_sink PRIVATE "src/")

target_compile_options(audio_sink PRIVATE -O3 -std=c++17)
target_link_libraries(audio_sink PRIVATE aaudio)