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

if (MSVC)
    target_compile_options(rigctl_server PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rigctl_server PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(rigctl_server PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS rigctl_server DESTINATION lib/sdrpp/plugins)
