cmake_minimum_required(VERSION 3.13)
project(airspy_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(airspy_source SHARED ${SRC})
target_link_libraries(airspy_source PRIVATE sdrpp_core)
set_target_properties(airspy_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(airspy_source PUBLIC airspy)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBAIRSPYHF REQUIRED libairspy)

    target_include_directories(airspy_source PUBLIC ${LIBAIRSPYHF_INCLUDE_DIRS})
    target_link_directories(airspy_source PUBLIC ${LIBAIRSPYHF_LIBRARY_DIRS})
    target_link_libraries(airspy_source PUBLIC ${LIBAIRSPYHF_LIBRARIES})
endif (MSVC)

# Install directives
install(TARGETS airspy_source DESTINATION lib/sdrpp/plugins)