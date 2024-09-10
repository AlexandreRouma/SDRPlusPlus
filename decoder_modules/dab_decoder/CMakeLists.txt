cmake_minimum_required(VERSION 3.13)
project(dab_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(dab_decoder PRIVATE "src/")

if (MSVC)
    # Lib path
    target_include_directories(dab_decoder PRIVATE "C:/Program Files/codec2/include/")
    target_link_directories(dab_decoder PRIVATE "C:/Program Files/codec2/lib")

    target_link_libraries(dab_decoder PRIVATE libcodec2)
elseif (ANDROID)
    target_include_directories(dab_decoder PUBLIC
        /sdr-kit/${ANDROID_ABI}/include/codec2
    )

    target_link_libraries(dab_decoder PUBLIC
        /sdr-kit/${ANDROID_ABI}/lib/libcodec2.so
    )
else ()
    find_package(PkgConfig)

    pkg_check_modules(LIBCODEC2 REQUIRED codec2)

    target_include_directories(dab_decoder PRIVATE ${LIBCODEC2_INCLUDE_DIRS})
    target_link_directories(dab_decoder PRIVATE ${LIBCODEC2_LIBRARY_DIRS})
    target_link_libraries(dab_decoder PRIVATE ${LIBCODEC2_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(dab_decoder PRIVATE "/usr/local/include")
    endif()
endif ()