cmake_minimum_required(VERSION 3.13)
project(spectran_source)

file(GLOB SRC "src/*.cpp")

add_library(spectran_source SHARED ${SRC})
target_link_libraries(spectran_source PRIVATE sdrpp_core)
set_target_properties(spectran_source PROPERTIES PREFIX "")

target_include_directories(spectran_source PRIVATE "src/")

if (MSVC)
    target_compile_options(spectran_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(spectran_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(spectran_source PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    # Lib path
    target_link_directories(spectran_source PRIVATE "C:/Program Files/Aaronia AG/Aaronia RTSA-Suite PRO/sdk")

    target_include_directories(spectran_source PUBLIC "C:/Program Files/Aaronia AG/Aaronia RTSA-Suite PRO/sdk")

    target_link_libraries(spectran_source PRIVATE AaroniaRTSAAPI)
else (MSVC)
    # Lib path
    target_link_directories(spectran_source PRIVATE "~/Aaronia/RTSA/Aaronia-RTSA-Suite-PRO")

    target_include_directories(spectran_source PUBLIC "~/Aaronia/RTSA/Aaronia-RTSA-Suite-PRO")

    target_link_libraries(spectran_source PRIVATE AaroniaRTSAAPI)
endif ()

# Install directives
install(TARGETS spectran_source DESTINATION lib/sdrpp/plugins)
