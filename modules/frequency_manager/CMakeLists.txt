cmake_minimum_required(VERSION 3.13)
project(frequency_manager)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

include_directories("src/" "../radio/src")

file(GLOB SRC "src/*.cpp")

add_library(frequency_manager SHARED ${SRC})
target_link_libraries(frequency_manager PRIVATE sdrpp_core)
set_target_properties(frequency_manager PROPERTIES PREFIX "")

# Install directives
install(TARGETS frequency_manager DESTINATION lib/sdrpp/plugins)