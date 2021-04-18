cmake_minimum_required(VERSION 3.13)
project(file_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wnounused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(file_source SHARED ${SRC})
target_link_libraries(file_source PRIVATE sdrpp_core)
set_target_properties(file_source PROPERTIES PREFIX "")

# Install directives
install(TARGETS file_source DESTINATION lib/sdrpp/plugins)