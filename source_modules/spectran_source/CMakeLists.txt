cmake_minimum_required(VERSION 3.13)
project(spectran_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(spectran_source PRIVATE "C:/Program Files/Aaronia AG/Aaronia RTSA-Suite PRO/sdk")

    target_include_directories(spectran_source PUBLIC "C:/Program Files/Aaronia AG/Aaronia RTSA-Suite PRO/sdk")

    target_link_libraries(spectran_source PRIVATE AaroniaRTSAAPI)
else (MSVC)
    # Lib path
    target_link_directories(spectran_source PRIVATE "/opt/aaronia-rtsa-suite/Aaronia-RTSA-Suite-PRO")

    target_include_directories(spectran_source PUBLIC "/opt/aaronia-rtsa-suite/Aaronia-RTSA-Suite-PRO/sdk")

    target_link_libraries(spectran_source PRIVATE AaroniaRTSAAPI)
endif ()
