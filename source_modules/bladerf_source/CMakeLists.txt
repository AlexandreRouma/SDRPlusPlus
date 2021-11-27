cmake_minimum_required(VERSION 3.13)
project(bladerf_source)

file(GLOB SRC "src/*.cpp")

add_library(bladerf_source SHARED ${SRC})
target_link_libraries(bladerf_source PRIVATE sdrpp_core)
set_target_properties(bladerf_source PROPERTIES PREFIX "")

target_include_directories(bladerf_source PRIVATE "src/")

if (MSVC)
    target_compile_options(bladerf_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(bladerf_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(bladerf_source PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    # Lib path
    target_link_directories(bladerf_source PRIVATE "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(bladerf_source PRIVATE bladeRF)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBBLADERF REQUIRED libbladeRF)

    target_include_directories(bladerf_source PRIVATE ${LIBBLADERF_INCLUDE_DIRS})
    target_link_directories(bladerf_source PRIVATE ${LIBBLADERF_LIBRARY_DIRS})
    target_link_libraries(bladerf_source PRIVATE ${LIBBLADERF_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(bladerf_source PRIVATE "/usr/local/include")
    endif()
endif ()

# Install directives
install(TARGETS bladerf_source DESTINATION lib/sdrpp/plugins)
