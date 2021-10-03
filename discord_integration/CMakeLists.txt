cmake_minimum_required(VERSION 3.13)
project(discord_integration)

if (MSVC)
    add_compile_options(/O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup -fPIC)
else ()
    add_compile_options(-O3 -std=c++17 -fPIC)
endif ()

add_subdirectory("discord-rpc")

file(GLOB SRC "src/*.cpp")
include_directories("src/" "discord-rpc/include" "../radio/src/")

add_library(discord_integration SHARED ${SRC})
target_link_libraries(discord_integration PUBLIC sdrpp_core discord-rpc)
set_target_properties(discord_integration PROPERTIES PREFIX "")

# Install directives
install(TARGETS discord_integration DESTINATION lib/sdrpp/plugins)