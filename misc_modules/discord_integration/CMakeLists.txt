cmake_minimum_required(VERSION 3.13)
project(discord_integration)

if (NOT MSVC)
    add_compile_options(-fPIC)
endif ()

add_subdirectory("discord-rpc")

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_link_libraries(discord_integration PRIVATE discord-rpc)
target_include_directories(discord_integration PRIVATE "src/" "discord-rpc/include" "../../decoder_modules/radio/src/")