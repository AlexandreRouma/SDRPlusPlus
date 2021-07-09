cmake_minimum_required(VERSION 3.13)
project(portaudio_sink)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(portaudio_sink SHARED ${SRC})
target_link_libraries(portaudio_sink PRIVATE sdrpp_core)
set_target_properties(portaudio_sink PROPERTIES PREFIX "")

if (MSVC)
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(portaudio_sink PUBLIC portaudio)
else (MSVC)
    find_package(PkgConfig)
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    target_include_directories(portaudio_sink PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_directories(portaudio_sink PRIVATE ${PORTAUDIO_LIBRARY_DIRS})
    target_link_libraries(portaudio_sink PRIVATE ${PORTAUDIO_LIBRARIES})
endif()

# Install directives
install(TARGETS portaudio_sink DESTINATION lib/sdrpp/plugins)