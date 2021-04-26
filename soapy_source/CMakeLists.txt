cmake_minimum_required(VERSION 3.10)
project(soapy_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB SRC "src/*.cpp")

add_library(soapy_source SHARED ${SRC})
target_link_libraries(soapy_source PRIVATE sdrpp_core)
set_target_properties(soapy_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(soapy_source PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(soapy_source PUBLIC "C:/Program Files/PothosSDR/include/")

    target_link_libraries(soapy_source PUBLIC SoapySDR)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(SOAPY REQUIRED SoapySDR)

    target_include_directories(soapy_source PUBLIC ${SOAPY_INCLUDE_DIRS})
    target_link_directories(soapy_source PUBLIC ${SOAPY_LIBRARY_DIRS})
    target_link_libraries(soapy_source PUBLIC ${SOAPY_LIBRARIES})
endif ()

# Install directives
install(TARGETS soapy_source DESTINATION lib/sdrpp/plugins)