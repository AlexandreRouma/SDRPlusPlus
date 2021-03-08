cmake_minimum_required(VERSION 3.13)
project(audio_sink)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(audio_sink SHARED ${SRC})
target_link_libraries(audio_sink PRIVATE sdrpp_core)
set_target_properties(audio_sink PROPERTIES PREFIX "")

if (MSVC)
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC portaudio)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

    target_include_directories(sdrpp_core PUBLIC ${PORTAUDIO_INCLUDE_DIRS})

    target_link_directories(sdrpp_core PUBLIC ${PORTAUDIO_LIBRARY_DIRS})

    target_link_libraries(sdrpp_core PUBLIC ${PORTAUDIO_LIBRARIES})

endif (MSVC)