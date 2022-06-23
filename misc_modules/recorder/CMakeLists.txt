cmake_minimum_required(VERSION 3.13)
project(recorder)

file(GLOB SRC "src/*.cpp")

add_library(recorder SHARED ${SRC})
target_link_libraries(recorder PRIVATE sdrpp_core)
set_target_properties(recorder PROPERTIES PREFIX "")

target_include_directories(recorder PRIVATE "src/")

# Install directives
install(TARGETS recorder DESTINATION lib/sdrpp/plugins)