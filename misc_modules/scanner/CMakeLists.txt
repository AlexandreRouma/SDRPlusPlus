cmake_minimum_required(VERSION 3.13)
project(scanner)

file(GLOB SRC "src/*.cpp")

add_library(scanner SHARED ${SRC})
target_link_libraries(scanner PRIVATE sdrpp_core)
set_target_properties(scanner PROPERTIES PREFIX "")

target_include_directories(scanner PRIVATE "src/")

# Install directives
install(TARGETS scanner DESTINATION lib/sdrpp/plugins)