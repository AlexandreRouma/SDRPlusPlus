cmake_minimum_required(VERSION 3.13)
project(rtl_tcp_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(rtl_tcp_source SHARED ${SRC})
target_link_libraries(rtl_tcp_source PRIVATE sdrpp_core)
set_target_properties(rtl_tcp_source PROPERTIES PREFIX "")

if(WIN32)
  target_link_libraries(rtl_tcp_source PRIVATE wsock32 ws2_32)
endif()

# Install directives
install(TARGETS rtl_tcp_source DESTINATION lib/sdrpp/plugins)