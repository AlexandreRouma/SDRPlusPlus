cmake_minimum_required(VERSION 3.13)
project(sdrpp_core)

# Set compiler options
if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    set(CMAKE_CXX_FLAGS "-g -O3 -std=c++17 -fpermissive")
endif (MSVC)
add_definitions(-DSDRPP_IS_CORE)

# Main code
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

# Add code to dyn lib
add_library(sdrpp_core SHARED ${SRC})

# Include core headers
target_include_directories(sdrpp_core PUBLIC "src/")
target_include_directories(sdrpp_core PUBLIC "src/imgui")


if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")

    # Volk
    target_link_libraries(sdrpp_core PUBLIC volk)
    
    # SoapySDR
    target_link_libraries(sdrpp_core PUBLIC SoapySDR)

    # Glew
    find_package(GLEW REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC GLEW::GLEW)

    # GLFW3
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC glfw)

    # FFTW3
    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC FFTW3::fftw3f)

    # PortAudio
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC portaudio portaudio_static)

    target_link_libraries(sdrpp_core PUBLIC volk)

else()
    find_package(PkgConfig)
    find_package(OpenGL REQUIRED)

    pkg_check_modules(GLEW REQUIRED glew)
    pkg_check_modules(FFTW3 REQUIRED fftw3f)
    pkg_check_modules(VOLK REQUIRED volk)
    pkg_check_modules(GLFW3 REQUIRED glfw3)
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

    target_include_directories(sdrpp_core PUBLIC
        ${GLEW_INCLUDE_DIRS}
        ${FFTW3_INCLUDE_DIRS}
        ${GLFW3_INCLUDE_DIRS}
        ${VOLK_INCLUDE_DIRS}
        ${PORTAUDIO_INCLUDE_DIRS}
        )

    target_link_directories(sdrpp_core PUBLIC
        ${GLEW_LIBRARY_DIRS}
        ${FFTW3_LIBRARY_DIRS}
        ${GLFW3_LIBRARY_DIRS}
        ${VOLK_LIBRARY_DIRS}
        ${PORTAUDIO_LIBRARY_DIRS}
        )

    target_link_libraries(sdrpp_core PUBLIC
        ${OPENGL_LIBRARIES}
        ${GLEW_STATIC_LIBRARIES}
        ${FFTW3_STATIC_LIBRARIES}
        ${GLFW3_STATIC_LIBRARIES}
        ${VOLK_STATIC_LIBRARIES}
        ${PORTAUDIO_STATIC_LIBRARIES}
        )

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(sdrpp_core PUBLIC stdc++fs)
    endif ()

    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif ()

endif ()

set(CORE_FILES ${RUNTIME_OUTPUT_DIRECTORY} PARENT_SCOPE)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"
