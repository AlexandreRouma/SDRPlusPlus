cmake_minimum_required(VERSION 3.13)
project(rtl_sdr_source)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fpermissive")
endif (MSVC)

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(rtl_sdr_source SHARED ${SRC})
target_link_libraries(rtl_sdr_source PRIVATE sdrpp_core)
set_target_properties(rtl_sdr_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(rtl_sdr_source PUBLIC rtlsdr)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBRTLSDR REQUIRED librtlsdr)

    target_include_directories(rtl_sdr_source PUBLIC ${LIBRTLSDR_INCLUDE_DIRS})
    target_link_directories(rtl_sdr_source PUBLIC ${LIBRTLSDR_LIBRARY_DIRS})
    target_link_libraries(rtl_sdr_source PUBLIC ${LIBRTLSDR_LIBRARIES})
endif (MSVC)

# Install directives
install(TARGETS rtl_sdr_source DESTINATION lib/sdrpp/plugins)