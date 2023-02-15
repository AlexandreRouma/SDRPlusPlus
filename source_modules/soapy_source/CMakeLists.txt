cmake_minimum_required(VERSION 3.13)
project(soapy_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(soapy_source PRIVATE "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(soapy_source PRIVATE "C:/Program Files/PothosSDR/include/")

    target_link_libraries(soapy_source PRIVATE SoapySDR)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(SOAPY REQUIRED SoapySDR)

    target_include_directories(soapy_source PRIVATE ${SOAPY_INCLUDE_DIRS})
    target_link_directories(soapy_source PRIVATE ${SOAPY_LIBRARY_DIRS})
    target_link_libraries(soapy_source PRIVATE ${SOAPY_LIBRARIES})
endif ()