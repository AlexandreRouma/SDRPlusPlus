cmake_minimum_required(VERSION 3.13)
project(kg_sstv_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

add_library(kg_sstv_decoder SHARED ${SRC})
target_link_libraries(kg_sstv_decoder PRIVATE sdrpp_core)
set_target_properties(kg_sstv_decoder PROPERTIES PREFIX "")

target_include_directories(kg_sstv_decoder PRIVATE "src/")

if (MSVC)
    target_compile_options(kg_sstv_decoder PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(kg_sstv_decoder PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(kg_sstv_decoder PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS kg_sstv_decoder DESTINATION lib/sdrpp/plugins)