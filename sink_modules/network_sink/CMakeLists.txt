cmake_minimum_required(VERSION 3.13)
project(network_sink)

file(GLOB SRC "src/*.cpp")

add_library(network_sink SHARED ${SRC})
target_link_libraries(network_sink PRIVATE sdrpp_core)
set_target_properties(network_sink PROPERTIES PREFIX "")

target_include_directories(network_sink PRIVATE "src/")

# Install directives
install(TARGETS network_sink DESTINATION lib/sdrpp/plugins)