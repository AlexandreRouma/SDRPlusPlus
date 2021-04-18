cmake_minimum_required(VERSION 3.13)
project(meteor_demodulator)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")
include_directories("src/libcorrect/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

add_library(meteor_demodulator SHARED ${SRC})
target_link_libraries(meteor_demodulator PRIVATE sdrpp_core)
set_target_properties(meteor_demodulator PROPERTIES PREFIX "")

# Install directives
install(TARGETS meteor_demodulator DESTINATION lib/sdrpp/plugins)