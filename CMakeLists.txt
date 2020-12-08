cmake_minimum_required(VERSION 3.13)
project(sdrpp)

# Core of SDR++
add_subdirectory("core")

# Cross platform modules
add_subdirectory("radio")
add_subdirectory("recorder") 
add_subdirectory("soapy_source")
#add_subdirectory("file_source")
add_subdirectory("rtl_tcp_source")
add_subdirectory("audio_sink")
#add_subdirectory("rx888_source")
add_subdirectory("plutosdr_source")
#add_subdirectory("demo")

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17")
else()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif (MSVC)

add_executable(sdrpp "src/main.cpp" "win32/resources.rc")
target_link_libraries(sdrpp PRIVATE sdrpp_core)

# Copy dynamic libs over
if (MSVC)
    add_custom_target(do_always ALL xcopy /s \"$<TARGET_FILE_DIR:sdrpp_core>\\*.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)
    add_custom_target(do_always_volk ALL xcopy /s \"C:/Program Files/PothosSDR/bin\\volk.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)
endif (MSVC)


if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.dylib\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"
