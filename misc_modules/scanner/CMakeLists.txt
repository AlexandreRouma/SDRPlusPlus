cmake_minimum_required(VERSION 3.13)
project(scanner)

if (MSVC)
    add_compile_options(/O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17)
endif ()

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(scanner SHARED ${SRC})
target_link_libraries(scanner PRIVATE sdrpp_core)
set_target_properties(scanner PROPERTIES PREFIX "")

# Install directives
install(TARGETS scanner DESTINATION lib/sdrpp/plugins)