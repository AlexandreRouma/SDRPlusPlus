cmake_minimum_required(VERSION 3.13)
project(rtl_sdr_source)

if (MSVC)
    add_compile_options(/O2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    add_compile_options(-O3 -std=c++17)
endif ()

include_directories("src/")

file(GLOB SRC "src/*.cpp")

add_library(rtl_sdr_source SHARED ${SRC})
target_link_libraries(rtl_sdr_source PRIVATE sdrpp_core)
set_target_properties(rtl_sdr_source PROPERTIES PREFIX "")

if (MSVC)
    # Lib path
    target_link_directories(rtl_sdr_source PUBLIC "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(rtl_sdr_source PUBLIC rtlsdr)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBRTLSDR REQUIRED librtlsdr)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

    target_include_directories(rtl_sdr_source PUBLIC ${LIBRTLSDR_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
    target_link_directories(rtl_sdr_source PUBLIC ${LIBRTLSDR_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})
    target_link_libraries(rtl_sdr_source PUBLIC ${LIBRTLSDR_LIBRARIES} ${LIBUSB_LIBRARIES})
endif ()

# Install directives
install(TARGETS rtl_sdr_source DESTINATION lib/sdrpp/plugins)