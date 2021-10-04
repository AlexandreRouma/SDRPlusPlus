cmake_minimum_required(VERSION 3.13)
project(new_portaudio_sink)

file(GLOB SRC "src/*.cpp")

add_library(new_portaudio_sink SHARED ${SRC})
target_link_libraries(new_portaudio_sink PRIVATE sdrpp_core)
set_target_properties(new_portaudio_sink PROPERTIES PREFIX "")

target_include_directories(new_portaudio_sink PRIVATE "src/")

if (MSVC)
    target_compile_options(new_portaudio_sink PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(new_portaudio_sink PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(new_portaudio_sink PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(new_portaudio_sink PRIVATE portaudio)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

    target_include_directories(new_portaudio_sink PRIVATE ${PORTAUDIO_INCLUDE_DIRS})

    target_link_directories(new_portaudio_sink PRIVATE ${PORTAUDIO_LIBRARY_DIRS})

    target_link_libraries(new_portaudio_sink PRIVATE ${PORTAUDIO_LIBRARIES})

endif (MSVC)

# Install directives
install(TARGETS new_portaudio_sink DESTINATION lib/sdrpp/plugins)