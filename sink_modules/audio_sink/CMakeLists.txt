cmake_minimum_required(VERSION 3.13)
project(audio_sink)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(audio_sink PRIVATE "C:/Program Files (x86)/RtAudio/lib")

    # Misc headers
    target_include_directories(audio_sink PRIVATE "C:/Program Files (x86)/RtAudio/include/rtaudio")

    target_link_libraries(audio_sink PRIVATE rtaudio)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(RTAUDIO REQUIRED rtaudio)

    target_include_directories(audio_sink PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_directories(audio_sink PRIVATE ${RTAUDIO_LIBRARY_DIRS})
    target_link_libraries(audio_sink PRIVATE ${RTAUDIO_LIBRARIES})

endif ()