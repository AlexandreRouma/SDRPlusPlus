cmake_minimum_required(VERSION 3.13)
project(rigctl_server)

file(GLOB SRC "src/*.cpp")

add_library(rigctl_server SHARED ${SRC})
target_link_libraries(rigctl_server PRIVATE sdrpp_core)
set_target_properties(rigctl_server PROPERTIES PREFIX "")

target_include_directories(rigctl_server PRIVATE "src/")
target_include_directories(rigctl_server PRIVATE "../recorder/src")
target_include_directories(rigctl_server PRIVATE "../../decoder_modules/meteor_demodulator/src")
target_include_directories(rigctl_server PRIVATE "../../decoder_modules/radio/src")

# Install directives
install(TARGETS rigctl_server DESTINATION lib/sdrpp/plugins)
