cmake_minimum_required(VERSION 3.13)
project(file_source)

file(GLOB SRC "src/*.cpp")

add_library(file_source SHARED ${SRC})
target_link_libraries(file_source PRIVATE sdrpp_core)
set_target_properties(file_source PROPERTIES PREFIX "")

target_include_directories(file_source PRIVATE "src/")

# Install directives
install(TARGETS file_source DESTINATION lib/sdrpp/plugins)