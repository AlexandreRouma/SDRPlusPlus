cmake_minimum_required(VERSION 3.13)
project(new_portaudio_sink)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

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