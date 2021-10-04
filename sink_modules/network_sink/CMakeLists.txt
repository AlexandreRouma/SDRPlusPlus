cmake_minimum_required(VERSION 3.13)
project(network_sink)

file(GLOB SRC "src/*.cpp")

add_library(network_sink SHARED ${SRC})
target_link_libraries(network_sink PRIVATE sdrpp_core)
set_target_properties(network_sink PROPERTIES PREFIX "")

target_include_directories(network_sink PRIVATE "src/")

if (MSVC)
    target_compile_options(network_sink PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(network_sink PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(network_sink PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS network_sink DESTINATION lib/sdrpp/plugins)