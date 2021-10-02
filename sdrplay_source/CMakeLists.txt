cmake_minimum_required(VERSION 3.13)
project(sdrplay_source)

if (MSVC)
    add_compile_options(/O2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17)
endif ()

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(sdrplay_source SHARED ${SRC})
target_link_libraries(sdrplay_source PRIVATE sdrpp_core)
set_target_properties(sdrplay_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrplay_source PUBLIC "C:/Program Files/SDRplay/API/x64")
    target_include_directories(sdrplay_source PUBLIC "C:/Program Files/SDRplay/API/inc")

    target_link_libraries(sdrplay_source PUBLIC sdrplay_api)
else (MSVC)
    find_package(PkgConfig)

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(sdrplay_source PUBLIC "/usr/local/include")
        target_link_directories(sdrplay_source PUBLIC "/usr/local/lib/")
    endif()

    target_link_libraries(sdrplay_source PRIVATE sdrplay_api)
endif ()

# Install directives
install(TARGETS sdrplay_source DESTINATION lib/sdrpp/plugins)