cmake_minimum_required(VERSION 3.9)
project(demo)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
    link_directories(demo "C:/Program Files/PothosSDR/lib/")
    include_directories(demo "C:/Program Files/PothosSDR/include/volk/")
    include_directories(demo "C:/Program Files/PothosSDR/include/")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fsanitize=address -g")
    include_directories(demo "/usr/include/volk")
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
include_directories(demo "src/")
include_directories(demo "../../src/")
include_directories(demo "../../src/imgui")
file(GLOB SRC "src/*.cpp")
file(GLOB IMGUI "../../src/imgui/*.cpp")
add_library(demo SHARED ${SRC} ${IMGUI})
set_target_properties(demo PROPERTIES OUTPUT_NAME demo)

if (MSVC)
    # Glew
    find_package(GLEW REQUIRED)
    target_link_libraries(demo PRIVATE GLEW::GLEW)

    # GLFW3
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(demo PRIVATE glfw)

    # FFTW3
    find_package(FFTW3 CONFIG REQUIRED)
    target_link_libraries(demo PRIVATE FFTW3::fftw3)
    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(demo PRIVATE FFTW3::fftw3f)

    # PortAudio
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(demo PRIVATE portaudio portaudio_static)
endif (MSVC)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"