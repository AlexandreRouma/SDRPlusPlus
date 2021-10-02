cmake_minimum_required(VERSION 3.13)
project(rigctl_server)

if (MSVC)
    add_compile_options(/O2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17)
endif ()

file(GLOB SRC "src/*.cpp")

include_directories("src/")
include_directories("../recorder/src")
include_directories("../meteor_demodulator/src")
include_directories("../radio/src")

add_library(rigctl_server SHARED ${SRC})
target_link_libraries(rigctl_server PRIVATE sdrpp_core)
set_target_properties(rigctl_server PROPERTIES PREFIX "")

# Install directives
install(TARGETS rigctl_server DESTINATION lib/sdrpp/plugins)
