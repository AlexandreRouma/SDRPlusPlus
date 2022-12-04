cmake_minimum_required(VERSION 3.13)
project(atv_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")

add_library(atv_decoder SHARED ${SRC})
target_link_libraries(atv_decoder PRIVATE sdrpp_core)
set_target_properties(atv_decoder PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(atv_decoder PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(atv_decoder PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(atv_decoder PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS atv_decoder DESTINATION lib/sdrpp/plugins)