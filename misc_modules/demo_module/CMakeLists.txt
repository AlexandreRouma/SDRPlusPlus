cmake_minimum_required(VERSION 3.13)
project(demo)

file(GLOB SRC "src/*.cpp")

add_library(demo SHARED ${SRC})
target_link_libraries(demo PRIVATE sdrpp_core)
set_target_properties(demo PROPERTIES PREFIX "")

target_include_directories(demo PRIVATE "src/")

# Install directives
install(TARGETS demo DESTINATION lib/sdrpp/plugins)