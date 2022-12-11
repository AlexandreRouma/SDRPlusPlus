cmake_minimum_required(VERSION 3.13)
project(redpitaya_source)

file(GLOB SRC "src/*.cpp")

add_library(redpitaya_source SHARED ${SRC})
target_link_libraries(redpitaya_source PRIVATE sdrpp_core)
set_target_properties(redpitaya_source PROPERTIES PREFIX "")

target_include_directories(redpitaya_source PRIVATE "src/")

if (MSVC)
    target_compile_options(redpitaya_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(redpitaya_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(redpitaya_source PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS redpitaya_source DESTINATION lib/sdrpp/plugins)
