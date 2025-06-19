cmake_minimum_required(VERSION 3.13)
project(hydrasdr_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    target_link_directories(hydrasdr_source PRIVATE "C:/Program Files/HydraSDR/")

    target_include_directories(hydrasdr_source PUBLIC "C:/Program Files/HydraSDR/include/")

    target_link_libraries(hydrasdr_source PRIVATE hydrasdr)
elseif (ANDROID)
    # TODO
else (MSVC)
    # TODO
endif ()