cmake_minimum_required(VERSION 3.9)
project(sdrpp)

# Compiler config
set(CMAKE_CXX_FLAGS "-OFast /std:c++17")

# HackRF
include_directories(sdrpp "C:/Program Files/PothosSDR/include/libhackrf/")
link_directories(sdrpp "C:/Program Files/PothosSDR/lib/")
link_libraries(hackrf)

# Main code
include_directories(sdrpp "src/")
include_directories(sdrpp "src/imgui")
file(GLOB SRC "src/*.cpp")
file(GLOB IMGUI "src/imgui/*.cpp")
add_executable(sdrpp ${SRC} ${IMGUI})

# Glew
find_package(GLEW REQUIRED)
target_link_libraries(sdrpp PRIVATE GLEW::GLEW)

# GLFW3
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(sdrpp PRIVATE glfw)

# FFTW3
find_package(FFTW3 CONFIG REQUIRED)
target_link_libraries(sdrpp PRIVATE FFTW3::fftw3)
find_package(FFTW3f CONFIG REQUIRED)
target_link_libraries(sdrpp PRIVATE FFTW3::fftw3f)
find_package(FFTW3l CONFIG REQUIRED)
target_link_libraries(sdrpp PRIVATE FFTW3::fftw3l)

# PortAudio
find_package(portaudio CONFIG REQUIRED)
target_link_libraries(sdrpp PRIVATE portaudio portaudio_static)