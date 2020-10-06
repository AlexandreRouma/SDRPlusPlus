cmake_minimum_required(VERSION 3.13)
project(sdrpp_core)

# Set compiler options
if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)
add_definitions(-DSDRPP_IS_CORE)

# Main code
file(GLOB SRC "src/*.cpp")
file(GLOB GUI "src/gui/*.cpp")
file(GLOB MENUS "src/gui/menus/*.cpp")
file(GLOB DIALOGS "src/gui/dialogs/*.cpp")
file(GLOB SIGPATH "src/signal_path/*.cpp")
file(GLOB IMGUI "src/imgui/*.cpp")
file(GLOB DUKTAPE "src/duktape/*.c")

# Add code to dyn lib
add_library(sdrpp_core SHARED ${SRC} ${GUI} ${MENUS} ${DIALOGS} ${SIGPATH} ${IMGUI} ${DUKTAPE})
set_target_properties(sdrpp_core PROPERTIES PREFIX "")

# Include core headers
target_include_directories(sdrpp_core PUBLIC "src/")
target_include_directories(sdrpp_core PUBLIC "src/imgui")


if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")

    # Volk
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/volk/")
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
    find_package(FFTW3 CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC FFTW3::fftw3)
    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC FFTW3::fftw3f)

    # PortAudio
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC portaudio portaudio_static)

else()
    target_include_directories(sdrpp_core PUBLIC "/usr/include/volk")

    target_link_libraries(sdrpp_core PUBLIC pthread)
    target_link_libraries(sdrpp_core PUBLIC GL)
    target_link_libraries(sdrpp_core PUBLIC GLEW)
    target_link_libraries(sdrpp_core PUBLIC glfw)
    target_link_libraries(sdrpp_core PUBLIC fftw3)
    target_link_libraries(sdrpp_core PUBLIC fftw3f)
    target_link_libraries(sdrpp_core PUBLIC portaudio)
    target_link_libraries(sdrpp_core PUBLIC X11)
    target_link_libraries(sdrpp_core PUBLIC Xxf86vm)
    target_link_libraries(sdrpp_core PUBLIC dl)
endif (MSVC)

target_link_libraries(sdrpp_core PUBLIC volk)

set(CORE_FILES ${RUNTIME_OUTPUT_DIRECTORY} PARENT_SCOPE)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"