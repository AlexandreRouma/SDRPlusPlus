cmake_minimum_required(VERSION 3.13)
project(weather_sat_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

add_library(weather_sat_decoder SHARED ${SRC})
target_link_libraries(weather_sat_decoder PRIVATE sdrpp_core)
set_target_properties(weather_sat_decoder PROPERTIES PREFIX "")

target_include_directories(weather_sat_decoder PRIVATE "src/")

if (MSVC)
    target_compile_options(weather_sat_decoder PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(weather_sat_decoder PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(weather_sat_decoder PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS weather_sat_decoder DESTINATION lib/sdrpp/plugins)