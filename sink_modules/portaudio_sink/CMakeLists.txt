cmake_minimum_required(VERSION 3.13)
project(audio_sink)

file(GLOB SRC "src/*.cpp")

add_library(audio_sink SHARED ${SRC})
target_link_libraries(audio_sink PRIVATE sdrpp_core)
set_target_properties(audio_sink PROPERTIES PREFIX "")

target_include_directories(audio_sink PRIVATE "src/")

if (MSVC)
    target_compile_options(audio_sink PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(audio_sink PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(audio_sink PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(audio_sink PRIVATE portaudio)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

    target_include_directories(audio_sink PRIVATE ${PORTAUDIO_INCLUDE_DIRS})

    target_link_directories(audio_sink PRIVATE ${PORTAUDIO_LIBRARY_DIRS})

    target_link_libraries(audio_sink PRIVATE ${PORTAUDIO_LIBRARIES})

endif (MSVC)

# Install directives
install(TARGETS audio_sink DESTINATION lib/sdrpp/plugins)