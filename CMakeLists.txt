cmake_minimum_required(VERSION 3.9)
project(sdrpp)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
    link_directories(sdrpp "C:/Program Files/PothosSDR/lib/")
    include_directories(sdrpp "C:/Program Files/PothosSDR/include/volk/")
    include_directories(sdrpp "C:/Program Files/PothosSDR/include/")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fsanitize=address -g")
    include_directories(sdrpp "/usr/include/volk")
    link_libraries(pthread)
    link_libraries(GL)
    link_libraries(GLEW)

    # GLFW3
    link_libraries(glfw)

    # FFTW3
    link_libraries(fftw3)
    link_libraries(fftw3f)

    # PortAudio
    link_libraries(portaudio)
endif (MSVC)

link_libraries(volk)
link_libraries(SoapySDR)

# Main code
include_directories(sdrpp "src/")
include_directories(sdrpp "src/imgui")
file(GLOB SRC "src/*.cpp")
file(GLOB IMGUI "src/imgui/*.cpp")
add_executable(sdrpp ${SRC} ${IMGUI})

if (MSVC)
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

    # PortAudio
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(sdrpp PRIVATE portaudio portaudio_static)
endif (MSVC)


# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"