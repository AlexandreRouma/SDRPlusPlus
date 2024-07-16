cmake_minimum_required(VERSION 3.13)
project(rfnm_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(rfnm_source PRIVATE "C:/Program Files/RFNM/lib/")
    target_include_directories(rfnm_source PUBLIC "C:/Program Files/RFNM/include/")
    target_link_libraries(rfnm_source PRIVATE rfnm)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBRFNM REQUIRED librfnm)

    target_include_directories(rfnm_source PRIVATE ${LIBRFNM_INCLUDE_DIRS})
    target_link_directories(rfnm_source PRIVATE ${LIBRFNM_LIBRARY_DIRS})
    target_link_libraries(rfnm_source PRIVATE ${LIBRFNM_LIBRARIES})
endif ()