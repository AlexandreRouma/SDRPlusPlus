cmake_minimum_required(VERSION 3.13)
project(recorder)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wnounused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(recorder SHARED ${SRC})
target_link_libraries(recorder PRIVATE sdrpp_core)
set_target_properties(recorder PROPERTIES PREFIX "")

# Install directives
install(TARGETS recorder DESTINATION lib/sdrpp/plugins)