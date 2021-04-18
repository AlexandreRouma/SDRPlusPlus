cmake_minimum_required(VERSION 3.13)
project(rx888_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

include_directories("src/CyAPI")

file(GLOB SRC "src/*.cpp")
file(GLOB CYAPI "src/CyAPI/*.cpp")

add_library(rx888_source SHARED ${SRC} ${CYAPI})
target_link_libraries(rx888_source PRIVATE sdrpp_core)
set_target_properties(rx888_source PROPERTIES PREFIX "")

# Install directives
install(TARGETS rx888_source DESTINATION lib/sdrpp/plugins)