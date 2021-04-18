cmake_minimum_required(VERSION 3.13)
project(audio_sink)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wnounused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(audio_sink SHARED ${SRC})
target_link_libraries(audio_sink PRIVATE sdrpp_core)
set_target_properties(audio_sink PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files (x86)/RtAudio/lib")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files (x86)/RtAudio/include/rtaudio")

    target_link_libraries(sdrpp_core PUBLIC rtaudio)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(RTAUDIO REQUIRED rtaudio)

    target_include_directories(sdrpp_core PUBLIC ${RTAUDIO_INCLUDE_DIRS})

    target_link_directories(sdrpp_core PUBLIC ${RTAUDIO_LIBRARY_DIRS})

    target_link_libraries(sdrpp_core PUBLIC ${RTAUDIO_LIBRARIES})

endif ()

# Install directives
install(TARGETS audio_sink DESTINATION lib/sdrpp/plugins)