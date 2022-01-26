cmake_minimum_required(VERSION 3.13)
project(sdrpp_server_source)

file(GLOB SRC "src/*.cpp")

add_library(sdrpp_server_source SHARED ${SRC})
target_link_libraries(sdrpp_server_source PRIVATE sdrpp_core)
set_target_properties(sdrpp_server_source PROPERTIES PREFIX "")

target_include_directories(sdrpp_server_source PRIVATE "src/")

if (MSVC)
    target_compile_options(sdrpp_server_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(sdrpp_server_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(sdrpp_server_source PRIVATE -O3 -std=c++17)
endif ()

if(WIN32)
  target_link_libraries(sdrpp_server_source PRIVATE wsock32 ws2_32)
endif()

# Install directives
install(TARGETS sdrpp_server_source DESTINATION lib/sdrpp/plugins)