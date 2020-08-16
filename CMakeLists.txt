cmake_minimum_required(VERSION 3.9)
project(sdrpp)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
    link_directories(sdrpp "C:/Program Files/PothosSDR/lib/")
    include_directories(sdrpp "C:/Program Files/PothosSDR/include/volk/")
    include_directories(sdrpp "C:/Program Files/PothosSDR/include/")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive -fsanitize=address -g")
    # set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
    include_directories(sdrpp "/usr/include/volk")
    link_libraries(pthread)
    link_libraries(GL)
    link_libraries(GLEW)
    link_libraries(glfw)
    link_libraries(fftw3)
    link_libraries(fftw3f)
    link_libraries(portaudio)
    link_libraries(X11)
    link_libraries(Xxf86vm)
    link_libraries(dl)
endif (MSVC)

link_libraries(volk)
link_libraries(SoapySDR)

# Main code
include_directories(sdrpp "src/")
include_directories(sdrpp "src/imgui")
file(GLOB SRC "src/*.cpp")
file(GLOB IMGUI "src/imgui/*.cpp")

# If on windows, set the executable icon
if (MSVC)
    set(SRC ${SRC} "win32/resources.rc")
endif (MSVC)

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

# # Copy resource directories
# if (!MSVC)
#     add_custom_command(TARGET sdrpp POST_BUILD COMMAND cmake -E copy_directory ${CMAKE_SOURCE_DIR}/res ${CMAKE_BINARY_DIR}/res)
#     add_custom_command(TARGET sdrpp POST_BUILD COMMAND cmake -E copy_directory ${CMAKE_SOURCE_DIR}/bandplans ${CMAKE_BINARY_DIR}/bandplans)
# endif (MSVC)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"