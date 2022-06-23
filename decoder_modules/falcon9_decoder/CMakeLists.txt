cmake_minimum_required(VERSION 3.13)
project(falcon9_decoder)

file(GLOB_RECURSE SRC "src/*.cpp")

add_library(falcon9_decoder SHARED ${SRC})
target_link_libraries(falcon9_decoder PRIVATE sdrpp_core)
set_target_properties(falcon9_decoder PROPERTIES PREFIX "")

target_include_directories(falcon9_decoder PRIVATE "src/")

# Install directives
install(TARGETS falcon9_decoder DESTINATION lib/sdrpp/plugins)