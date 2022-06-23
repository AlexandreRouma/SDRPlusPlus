cmake_minimum_required(VERSION 3.13)
project(discord_integration)

if (MSVC)
    # Nothing to add
else ()
    add_compile_options(-fPIC)
endif ()

add_subdirectory("discord-rpc")

file(GLOB SRC "src/*.cpp")

add_library(discord_integration SHARED ${SRC})
target_link_libraries(discord_integration PRIVATE sdrpp_core discord-rpc)
set_target_properties(discord_integration PROPERTIES PREFIX "")

target_include_directories(discord_integration PRIVATE "src/" "discord-rpc/include" "../../decoder_modules/radio/src/")

# Install directives
install(TARGETS discord_integration DESTINATION lib/sdrpp/plugins)