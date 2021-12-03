cmake_minimum_required(VERSION 3.13)
project(new_radio)

file(GLOB_RECURSE SRC "src/*.cpp")

add_library(new_radio SHARED ${SRC})
target_link_libraries(new_radio PRIVATE sdrpp_core)
set_target_properties(new_radio PROPERTIES PREFIX "")

target_include_directories(new_radio PRIVATE "src/")

if (MSVC)
    target_compile_options(new_radio PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(new_radio PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(new_radio PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS new_radio DESTINATION lib/sdrpp/plugins)