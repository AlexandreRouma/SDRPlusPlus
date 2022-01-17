cmake_minimum_required(VERSION 3.13)
project(uhd_source)

set(SOURCES
        src/device.cpp
        src/main.cpp
        )

add_library(uhd_source SHARED ${SOURCES})
target_link_libraries(uhd_source PRIVATE sdrpp_core)
set_target_properties(uhd_source PROPERTIES PREFIX "")

target_include_directories(uhd_source PRIVATE "src/")

if (MSVC)
    target_compile_options(uhd_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(uhd_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(uhd_source PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    message(FATAL_ERROR "implement...")
else (MSVC)
    find_package(UHD)
    target_link_libraries(uhd_source PRIVATE uhd)
endif ()

# Install directives
install(TARGETS uhd_source DESTINATION lib/sdrpp/plugins)