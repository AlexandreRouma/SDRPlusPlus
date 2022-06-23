cmake_minimum_required(VERSION 3.13)
project(soapy_source)

file(GLOB SRC "src/*.cpp")

add_library(soapy_source SHARED ${SRC})
target_link_libraries(soapy_source PRIVATE sdrpp_core)
set_target_properties(soapy_source PROPERTIES PREFIX "")

target_include_directories(soapy_source PRIVATE "src/")

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

# Install directives
install(TARGETS soapy_source DESTINATION lib/sdrpp/plugins)