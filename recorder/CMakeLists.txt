cmake_minimum_required(VERSION 3.13)
project(recorder)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
    link_directories(recorder "C:/Program Files/PothosSDR/lib/")
    include_directories(recorder "C:/Program Files/PothosSDR/include/volk/")
    include_directories(recorder "C:/Program Files/PothosSDR/include/")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fsanitize=address -g")
    include_directories(recorder "/usr/include/volk")
    link_libraries(pthread)
    link_libraries(GL)
    link_libraries(GLEW)
    link_libraries(glfw)
    link_libraries(fftw3)
    link_libraries(fftw3f)
    link_libraries(portaudio)
    link_libraries(X11)
    link_libraries(Xxf86vm)
endif (MSVC)

link_libraries(volk)
link_libraries(SoapySDR)

# Main code
include_directories(recorder "src/")
include_directories(recorder "../core/src/")
include_directories(recorder "../core/src/imgui")
file(GLOB SRC "src/*.cpp")
file(GLOB IMGUI "../core/src/imgui/*.cpp")
add_library(recorder SHARED ${SRC} ${IMGUI})
set_target_properties(recorder PROPERTIES PREFIX "")

if (MSVC)
    # Glew
    find_package(GLEW REQUIRED)
    target_link_libraries(recorder PRIVATE GLEW::GLEW)

    # GLFW3
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(recorder PRIVATE glfw)

    # FFTW3
    find_package(FFTW3 CONFIG REQUIRED)
    target_link_libraries(recorder PRIVATE FFTW3::fftw3)
    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(recorder PRIVATE FFTW3::fftw3f)

    # PortAudio
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(recorder PRIVATE portaudio portaudio_static)
endif (MSVC)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"