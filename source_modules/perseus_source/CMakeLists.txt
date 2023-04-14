cmake_minimum_required(VERSION 3.13)
project(perseus_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Lib path
    target_link_directories(perseus_source PRIVATE "C:/Users/ryzerth/Documents/Code/libperseus-sdr/build/Debug")

    target_include_directories(perseus_source PUBLIC "C:/Users/ryzerth/Documents/Code/libperseus-sdr/src")

    target_link_libraries(perseus_source PRIVATE perseus-sdr)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBPERSEUSSDR REQUIRED libperseus-sdr)

    target_include_directories(perseus_source PRIVATE ${LIBPERSEUSSDR_INCLUDE_DIRS})
    target_link_directories(perseus_source PRIVATE ${LIBPERSEUSSDR_LIBRARY_DIRS})
    target_link_libraries(perseus_source PRIVATE ${LIBPERSEUSSDR_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(perseus_source PRIVATE "/usr/local/include")
    endif()
    
endif ()