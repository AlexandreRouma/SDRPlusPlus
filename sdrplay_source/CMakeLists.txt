cmake_minimum_required(VERSION 3.13)
project(sdrplay_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wnounused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(sdrplay_source SHARED ${SRC})
target_link_libraries(sdrplay_source PRIVATE sdrpp_core)
set_target_properties(sdrplay_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/SDRplay/API/x64")
    target_include_directories(sdrplay_source PUBLIC "C:/Program Files/SDRplay/API/inc")

    target_link_libraries(sdrplay_source PUBLIC sdrplay_api)
else (MSVC)
    find_package(PkgConfig)

    target_link_libraries(sdrplay_source PRIVATE sdrplay_api)
endif ()

# Install directives
install(TARGETS sdrplay_source DESTINATION lib/sdrpp/plugins)