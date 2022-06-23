cmake_minimum_required(VERSION 3.13)
project(meteor_demodulator)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")

add_library(meteor_demodulator SHARED ${SRC})
target_link_libraries(meteor_demodulator PRIVATE sdrpp_core)
set_target_properties(meteor_demodulator PROPERTIES PREFIX "")

# Install directives
install(TARGETS meteor_demodulator DESTINATION lib/sdrpp/plugins)