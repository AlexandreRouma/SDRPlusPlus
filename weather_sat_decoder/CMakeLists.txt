cmake_minimum_required(VERSION 3.10)
project(weather_sat_decoder)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
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