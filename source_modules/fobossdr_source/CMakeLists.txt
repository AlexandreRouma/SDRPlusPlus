cmake_minimum_required(VERSION 3.13)
project(fobossdr_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(fobossdr_source PRIVATE "C:/Program Files/libfobos/lib/")
    target_include_directories(fobossdr_source PRIVATE "C:/Program Files/libfobos/include/")
    target_link_libraries(fobossdr_source PRIVATE fobos)
else (MSVC)
    # TODO
endif ()