cmake_minimum_required(VERSION 3.13)
project(limesdr_source)

file(GLOB SRC "src/*.cpp")

add_library(limesdr_source SHARED ${SRC})
target_link_libraries(limesdr_source PRIVATE sdrpp_core)
set_target_properties(limesdr_source PROPERTIES PREFIX "")

target_include_directories(limesdr_source PRIVATE "src/")

if (MSVC)
    # Lib path
    target_link_directories(limesdr_source PRIVATE "C:/Program Files/PothosSDR/lib")

    target_link_libraries(limesdr_source PRIVATE LimeSuite)
else (MSVC)
    find_package(PkgConfig)

    # Not in pkg-config
    target_link_libraries(limesdr_source PRIVATE LimeSuite)

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(airspyhf_source PRIVATE "/usr/local/include")
    endif()
endif ()

# Install directives
install(TARGETS limesdr_source DESTINATION lib/sdrpp/plugins)