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

if (MSVC)
    if ( NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(discord_integration PRIVATE /Ob2 /std:c++17 /EHsc)
    else()
        target_compile_options(discord_integration PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
    endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(discord_integration PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(discord_integration PRIVATE -O3 -std=c++17)
endif ()

# Install directives
install(TARGETS discord_integration DESTINATION lib/sdrpp/plugins)
