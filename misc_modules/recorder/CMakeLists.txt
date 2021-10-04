cmake_minimum_required(VERSION 3.13)
project(recorder)

file(GLOB SRC "src/*.cpp")

add_library(recorder SHARED ${SRC})
target_link_libraries(recorder PRIVATE sdrpp_core)
set_target_properties(recorder PROPERTIES PREFIX "")

target_include_directories(recorder PRIVATE "src/")

if (MSVC)
    target_compile_options(recorder PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(recorder PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(recorder PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS recorder DESTINATION lib/sdrpp/plugins)