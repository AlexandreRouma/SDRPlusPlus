cmake_minimum_required(VERSION 3.13)
project(weather_sat_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

add_library(weather_sat_decoder SHARED ${SRC})
target_link_libraries(weather_sat_decoder PRIVATE sdrpp_core)
set_target_properties(weather_sat_decoder PROPERTIES PREFIX "")

target_include_directories(weather_sat_decoder PRIVATE "src/")

# Install directives
install(TARGETS weather_sat_decoder DESTINATION lib/sdrpp/plugins)