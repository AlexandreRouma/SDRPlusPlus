cmake_minimum_required(VERSION 3.13)
project(audio_sink)

if (MSVC)
    add_compile_options(/O2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17 -fpermissive)
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

# Install directives
install(TARGETS audio_sink DESTINATION lib/sdrpp/plugins)