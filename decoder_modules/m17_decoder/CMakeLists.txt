cmake_minimum_required(VERSION 3.13)
project(m17_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

add_library(m17_decoder SHARED ${SRC})
target_link_libraries(m17_decoder PRIVATE sdrpp_core)
set_target_properties(m17_decoder PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(m17_decoder PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(m17_decoder PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(m17_decoder PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    # Lib path
    target_include_directories(m17_decoder PUBLIC "C:/Program Files/codec2/include/")
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/codec2/lib")

    target_link_libraries(m17_decoder PRIVATE libcodec2)

else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBCODEC2 REQUIRED codec2)

    target_include_directories(m17_decoder PUBLIC ${LIBCODEC2_INCLUDE_DIRS})
    target_link_directories(m17_decoder PUBLIC ${LIBCODEC2_LIBRARY_DIRS})
    target_link_libraries(m17_decoder PUBLIC ${LIBCODEC2_LIBRARIES})

    # Include it because for some reason pkgconfig doesn't look here?
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(m17_decoder PUBLIC "/usr/local/include")
    endif()

endif ()



# Install directives
install(TARGETS m17_decoder DESTINATION lib/sdrpp/plugins)