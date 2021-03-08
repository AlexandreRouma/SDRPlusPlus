cmake_minimum_required(VERSION 3.00)
project(ArcaleServer_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

include_directories("src/")
include_directories(${tinyxml_INCLUDE})

file(GLOB SRC "src/*.cpp")

add_library(ArcaleServer_source SHARED ${SRC}  "src/ArcaleRFServer.h")
target_link_libraries(ArcaleServer_source PRIVATE sdrpp_core)
set_target_properties(ArcaleServer_source PROPERTIES PREFIX "")

if(WIN32)
    message(DCMAKE_BUILD_TYPE)
    if (DCMAKE_BUILD_TYPE EQUAL "DEBUG")
        target_link_libraries(ArcaleServer_source PRIVATE wsock32 ws2_32 ${tinyxml_LIBRARY_DEBUG}/Tinyxml.lib)
        message("tinyxml debug")
    else()
        target_link_libraries(ArcaleServer_source PRIVATE wsock32 ws2_32 ${tinyxml_LIBRARY}/Tinyxml.lib)
        message("tinyxml release")
    endif()
endif()