cmake_minimum_required(VERSION 3.13)
project(scheduler)

file(GLOB SRC "src/*.cpp")

add_library(scheduler SHARED ${SRC})
target_link_libraries(scheduler PRIVATE sdrpp_core)
set_target_properties(scheduler PROPERTIES PREFIX "")

target_include_directories(scheduler PRIVATE "src/")

# Install directives
install(TARGETS scheduler DESTINATION lib/sdrpp/plugins)