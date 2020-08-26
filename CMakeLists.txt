cmake_minimum_required(VERSION 3.9)
project(sdrpp)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
    link_directories(sdrpp "C:/Program Files/PothosSDR/lib/")
    include_directories(sdrpp "C:/Program Files/PothosSDR/include/volk/")
    include_directories(sdrpp "C:/Program Files/PothosSDR/include/")
    link_directories(sdrpp "C:/Program Files/PothosSDR/lib/")
ENDIF()

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
# add_library(sdrpp ${SRC} ${IMGUI})

if (MSVC)
    # Glew
    find_package(GLEW REQUIRED)

    # GLFW3
    find_package(glfw3 CONFIG REQUIRED)

    # FFTW3
    find_package(FFTW3 CONFIG REQUIRED)

    find_package(FFTW3f CONFIG REQUIRED)

    # PortAudio
    find_package(portaudio CONFIG REQUIRED)
endif (MSVC)

FUNCTION(LINK_LIB)
    target_link_libraries(${ARGV0} PRIVATE GLEW::GLEW)
    target_link_libraries(${ARGV0} PRIVATE glfw)
    target_link_libraries(${ARGV0} PRIVATE FFTW3::fftw3)
    target_link_libraries(${ARGV0} PRIVATE FFTW3::fftw3f)
    target_link_libraries(${ARGV0} PRIVATE portaudio portaudio_static)
ENDFUNCTION()

FUNCTION(AddPlugin)
    file(GLOB PLUGIN_MOD_SRC "modules/${ARGV0}/src/*.cpp")
    include_directories(${ARGV0} "modules/${ARGV0}/src")
    add_library(${ARGV0} SHARED ${PLUGIN_MOD_SRC} ${IMGUI})
    LINK_LIB(${ARGV0})
    set_target_properties(${ARGV0} PROPERTIES OUTPUT_NAME ${ARGV0})
ENDFUNCTION()

# Main code
include_directories(sdrpp "src/")
include_directories(sdrpp "src/imgui")
file(GLOB SRC "src/*.cpp")
file(GLOB IMGUI "src/imgui/*.cpp")

# If on windows, set the executable icon
if (MSVC)
    set(SRC ${SRC} "win32/resources.rc")
endif (MSVC)

add_executable(sdrpp ${SRC} ${IMGUI} ${RX888})
LINK_LIB(sdrpp)

AddPlugin(radio)
AddPlugin(demo)
AddPlugin(recorder)

# # Copy resource directories
# if (!MSVC)
#     add_custom_command(TARGET sdrpp POST_BUILD COMMAND cmake -E copy_directory ${CMAKE_SOURCE_DIR}/res ${CMAKE_BINARY_DIR}/res)
#     add_custom_command(TARGET sdrpp POST_BUILD COMMAND cmake -E copy_directory ${CMAKE_SOURCE_DIR}/bandplans ${CMAKE_BINARY_DIR}/bandplans)
# endif (MSVC)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"