cmake_minimum_required(VERSION 3.13)
project(sdrpp_core)

# Cross platform modules
add_subdirectory("core")
add_subdirectory("radio")
add_subdirectory("recorder") 
add_subdirectory("soapy")
add_subdirectory("file_source")
add_subdirectory("rtl_tcp_source")
add_subdirectory("demo")

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
else()
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/sdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif (MSVC)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Alex/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 15 2017 Win64"
