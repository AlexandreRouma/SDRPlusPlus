cmake_minimum_required(VERSION 3.13)
project(radio)

file(GLOB_RECURSE SRC "src/*.cpp")

add_library(radio SHARED ${SRC})
target_link_libraries(radio PRIVATE sdrpp_core)
set_target_properties(radio PROPERTIES PREFIX "")

target_include_directories(radio PRIVATE "src/")

# Install directives
install(TARGETS radio DESTINATION lib/sdrpp/plugins)