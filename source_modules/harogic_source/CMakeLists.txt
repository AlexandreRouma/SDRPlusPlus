cmake_minimum_required(VERSION 3.13)
project(harogic_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(harogic_source PRIVATE "C:/radio/HTRA_API/x64/htra_api/")
    target_include_directories(harogic_source PRIVATE "C:/radio/HTRA_API/x64/htra_api/")
    target_link_libraries(harogic_source PRIVATE htra_api)
else (MSVC)
    target_link_directories(harogic_source PRIVATE "/opt/htraapi/lib/${CMAKE_SYSTEM_PROCESSOR}/")
    target_include_directories(harogic_source PRIVATE "/opt/htraapi/inc/")
    target_link_libraries(harogic_source PRIVATE htraapi)
endif ()