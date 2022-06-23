cmake_minimum_required(VERSION 3.13)
project(kg_sstv_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

add_library(kg_sstv_decoder SHARED ${SRC})
target_link_libraries(kg_sstv_decoder PRIVATE sdrpp_core)
set_target_properties(kg_sstv_decoder PROPERTIES PREFIX "")

target_include_directories(kg_sstv_decoder PRIVATE "src/")

# Install directives
install(TARGETS kg_sstv_decoder DESTINATION lib/sdrpp/plugins)