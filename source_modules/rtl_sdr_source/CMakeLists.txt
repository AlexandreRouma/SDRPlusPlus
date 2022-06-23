cmake_minimum_required(VERSION 3.13)
project(rtl_sdr_source)

file(GLOB SRC "src/*.cpp")

add_library(rtl_sdr_source SHARED ${SRC})
target_link_libraries(rtl_sdr_source PRIVATE sdrpp_core)
set_target_properties(rtl_sdr_source PROPERTIES PREFIX "")

target_include_directories(rtl_sdr_source PRIVATE "src/")

if (MSVC)
    target_compile_options(rtl_sdr_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rtl_sdr_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(rtl_sdr_source PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    # Lib path
    target_link_directories(rtl_sdr_source PRIVATE "C:/Program Files/PothosSDR/bin/")

    target_link_libraries(rtl_sdr_source PRIVATE rtlsdr)
elseif (ANDROID)
    target_include_directories(rtl_sdr_source PUBLIC
        /mnt/android_sdr/libusb/libusb
        /mnt/android_sdr/librtlsdr/include
    )

    target_link_libraries(rtl_sdr_source PUBLIC
        /mnt/android_sdr/output/libusb/${ANDROID_ABI}/libusb1.0.so
        /mnt/android_sdr/output/librtlsdr/${ANDROID_ABI}/librtlsdr.so
    )
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBRTLSDR REQUIRED librtlsdr)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

    target_include_directories(rtl_sdr_source PRIVATE ${LIBRTLSDR_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
    target_link_directories(rtl_sdr_source PRIVATE ${LIBRTLSDR_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})
    target_link_libraries(rtl_sdr_source PRIVATE ${LIBRTLSDR_LIBRARIES} ${LIBUSB_LIBRARIES})
endif ()

# Install directives
install(TARGETS rtl_sdr_source DESTINATION lib/sdrpp/plugins)