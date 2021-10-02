cmake_minimum_required(VERSION 3.13)
project(radio)

if (MSVC)
    add_compile_options(/O2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17)
endif ()

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(radio SHARED ${SRC})
target_link_libraries(radio PRIVATE sdrpp_core)
set_target_properties(radio PROPERTIES PREFIX "")

# Install directives
install(TARGETS radio DESTINATION lib/sdrpp/plugins)