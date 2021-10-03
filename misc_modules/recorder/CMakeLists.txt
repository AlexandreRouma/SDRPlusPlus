cmake_minimum_required(VERSION 3.13)
project(recorder)

if (MSVC)
    add_compile_options(/O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17)
endif ()

file(GLOB SRC "src/*.cpp")

include_directories("src/")

add_library(recorder SHARED ${SRC})
target_link_libraries(recorder PRIVATE sdrpp_core)
set_target_properties(recorder PROPERTIES PREFIX "")

# Install directives
install(TARGETS recorder DESTINATION lib/sdrpp/plugins)