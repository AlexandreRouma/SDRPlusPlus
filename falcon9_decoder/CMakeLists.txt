cmake_minimum_required(VERSION 3.13)
project(falcon9_decoder)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")
include_directories("src/libcorrect/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

add_library(falcon9_decoder SHARED ${SRC})
target_link_libraries(falcon9_decoder PRIVATE sdrpp_core)
set_target_properties(falcon9_decoder PROPERTIES PREFIX "")

# Install directives
install(TARGETS falcon9_decoder DESTINATION lib/sdrpp/plugins)