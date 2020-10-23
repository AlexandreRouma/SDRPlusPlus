cmake_minimum_required(VERSION 3.13)
project(soapy)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")

    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

file(GLOB SRC "src/*.cpp")

add_library(soapy SHARED ${SRC})
target_link_libraries(soapy PRIVATE sdrpp_core)
set_target_properties(soapy PROPERTIES PREFIX "")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_package(PkgConfig)
    find_package(OpenGL REQUIRED)

    pkg_check_modules(SOAPY REQUIRED soapysdr)

    target_include_directories(soapy PUBLIC ${SOAPY_INCLUDE_DIRS})
    target_link_directories(soapy PUBLIC ${SOAPY_LIBRARY_DIRS})
    target_link_libraries(soapy PUBLIC ${SOAPY_LIBRARIES})
else ()
    target_link_libraries(soapy PUBLIC SoapySDR)
endif ()
