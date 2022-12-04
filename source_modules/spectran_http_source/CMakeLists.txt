cmake_minimum_required(VERSION 3.13)
project(spectran_http_source)

file(GLOB_RECURSE SRC "src/*.cpp")

add_library(spectran_http_source SHARED ${SRC})
target_link_libraries(spectran_http_source PRIVATE sdrpp_core)
set_target_properties(spectran_http_source PROPERTIES PREFIX "")

target_include_directories(spectran_http_source PRIVATE "src/")

if (MSVC)
    target_compile_options(spectran_http_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(spectran_http_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(spectran_http_source PRIVATE -O3 -std=c++17)
endif ()

if(WIN32)
    target_link_libraries(spectran_http_source PRIVATE wsock32 ws2_32 iphlpapi)
endif()

# Install directives
install(TARGETS spectran_http_source DESTINATION lib/sdrpp/plugins)