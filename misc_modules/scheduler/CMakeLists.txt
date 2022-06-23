cmake_minimum_required(VERSION 3.13)
project(scheduler)

file(GLOB SRC "src/*.cpp")

add_library(scheduler SHARED ${SRC})
target_link_libraries(scheduler PRIVATE sdrpp_core)
set_target_properties(scheduler PROPERTIES PREFIX "")

target_include_directories(scheduler PRIVATE "src/")

if (MSVC)
    target_compile_options(scheduler PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(scheduler PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(scheduler PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS scheduler DESTINATION lib/sdrpp/plugins)