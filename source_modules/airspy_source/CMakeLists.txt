cmake_minimum_required(VERSION 3.13)
project(airspy_source)

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(airspy_source SHARED ${SRC})
target_link_libraries(airspy_source PRIVATE sdrpp_core)
set_target_properties(airspy_source PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(airspy_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(airspy_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(airspy_source PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    # Lib path
    target_link_directories(airspy_source PUBLIC "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(airspy_source PUBLIC airspy)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBAIRSPY REQUIRED libairspy)

    target_include_directories(airspy_source PUBLIC ${LIBAIRSPY_INCLUDE_DIRS})
    target_link_directories(airspy_source PUBLIC ${LIBAIRSPY_LIBRARY_DIRS})
    target_link_libraries(airspy_source PUBLIC ${LIBAIRSPY_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(airspy_source PUBLIC "/usr/local/include")
    endif()

endif ()

# Install directives
install(TARGETS airspy_source DESTINATION lib/sdrpp/plugins)