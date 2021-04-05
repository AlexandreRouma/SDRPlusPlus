cmake_minimum_required(VERSION 3.13)
project(libsddc_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB SRC "src/*.cpp")

add_library(libsddc_source SHARED ${SRC})
target_link_libraries(libsddc_source PRIVATE sdrpp_core)
set_target_properties(libsddc_source PROPERTIES PREFIX "")

if (NOT MSVC)
  target_link_libraries(libsddc_source PUBLIC usb-1.0)
endif (NOT MSVC)

target_link_libraries(libsddc_source PUBLIC sddc)

# Install directives
install(TARGETS libsddc_source DESTINATION lib/sdrpp/plugins)
