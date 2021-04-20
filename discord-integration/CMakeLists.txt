cmake_minimum_required(VERSION 3.13)
project(discord-integration)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup -fPIC")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fPIC")
endif ()

add_subdirectory("discord-rpc")

file(GLOB SRC "src/*.cpp")
include_directories("src/" "discord-rpc/include")

add_library(discord-integration SHARED ${SRC})
target_link_libraries(discord-integration PUBLIC sdrpp_core discord-rpc)
set_target_properties(discord-integration PROPERTIES PREFIX "")

# Install directives
install(TARGETS discord-integration DESTINATION lib/sdrpp/plugins)