cmake_minimum_required(VERSION 3.13)
project(spyserver_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wnounused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(spyserver_source SHARED ${SRC})
target_link_libraries(spyserver_source PRIVATE sdrpp_core)
set_target_properties(spyserver_source PROPERTIES PREFIX "")

if(WIN32)
  target_link_libraries(spyserver_source PRIVATE wsock32 ws2_32)
endif()

# Install directives
install(TARGETS spyserver_source DESTINATION lib/sdrpp/plugins)