cmake_minimum_required(VERSION 3.13)
project(demo)

file(GLOB SRC "src/*.cpp")

add_library(demo SHARED ${SRC})
target_link_libraries(demo PRIVATE sdrpp_core)
set_target_properties(demo PROPERTIES PREFIX "")

target_include_directories(demo PRIVATE "src/")

if (MSVC)
    target_compile_options(demo PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(demo PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(demo PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS demo DESTINATION lib/sdrpp/plugins)