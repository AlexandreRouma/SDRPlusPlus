cmake_minimum_required(VERSION 3.13)
project(frequency_manager)

file(GLOB SRC "src/*.cpp")

add_library(frequency_manager SHARED ${SRC})
target_link_libraries(frequency_manager PRIVATE sdrpp_core)
set_target_properties(frequency_manager PROPERTIES PREFIX "")

target_include_directories(frequency_manager PRIVATE "src/" "../../decoder_modules/radio/src")

# Install directives
install(TARGETS frequency_manager DESTINATION lib/sdrpp/plugins)