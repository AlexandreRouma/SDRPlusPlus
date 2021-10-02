cmake_minimum_required(VERSION 3.13)
project(weather_sat_decoder)

if (MSVC)
    add_compile_options(/O2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17)
endif ()

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")
include_directories("src/libcorrect/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

add_library(weather_sat_decoder SHARED ${SRC})
target_link_libraries(weather_sat_decoder PRIVATE sdrpp_core)
set_target_properties(weather_sat_decoder PROPERTIES PREFIX "")

# Install directives
install(TARGETS weather_sat_decoder DESTINATION lib/sdrpp/plugins)