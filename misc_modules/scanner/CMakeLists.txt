cmake_minimum_required(VERSION 3.13)
project(scanner)

file(GLOB SRC "src/*.cpp")

add_library(scanner SHARED ${SRC})
target_link_libraries(scanner PRIVATE sdrpp_core)
set_target_properties(scanner PROPERTIES PREFIX "")

target_include_directories(scanner PRIVATE "src/")

if (MSVC)
    target_compile_options(scanner PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(scanner PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(scanner PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS scanner DESTINATION lib/sdrpp/plugins)