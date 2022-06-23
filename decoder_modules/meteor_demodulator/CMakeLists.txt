cmake_minimum_required(VERSION 3.13)
project(meteor_demodulator)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")

add_library(meteor_demodulator SHARED ${SRC})
target_link_libraries(meteor_demodulator PRIVATE sdrpp_core)
set_target_properties(meteor_demodulator PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(meteor_demodulator PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(meteor_demodulator PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(meteor_demodulator PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS meteor_demodulator DESTINATION lib/sdrpp/plugins)