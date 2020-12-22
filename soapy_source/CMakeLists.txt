cmake_minimum_required(VERSION 3.13)
project(soapy_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

file(GLOB SRC "src/*.cpp")

add_library(soapy_source SHARED ${SRC})
target_link_libraries(soapy_source PRIVATE sdrpp_core)
set_target_properties(soapy_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")

    target_link_libraries(sdrpp_core PUBLIC SoapySDR)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(SOAPY REQUIRED SoapySDR)

    target_include_directories(soapy_source PUBLIC ${SOAPY_INCLUDE_DIRS})
    target_link_directories(soapy_source PUBLIC ${SOAPY_LIBRARY_DIRS})
    target_link_libraries(soapy_source PUBLIC ${SOAPY_LIBRARIES})
endif (MSVC)