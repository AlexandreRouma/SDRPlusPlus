cmake_minimum_required(VERSION 3.13)
project(frequency_manager)

include_directories("src/" "../../decoder_modules/radio/src")

file(GLOB SRC "src/*.cpp")

add_library(frequency_manager SHARED ${SRC})
target_link_libraries(frequency_manager PRIVATE sdrpp_core)
set_target_properties(frequency_manager PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(frequency_manager PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(frequency_manager PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(frequency_manager PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS frequency_manager DESTINATION lib/sdrpp/plugins)