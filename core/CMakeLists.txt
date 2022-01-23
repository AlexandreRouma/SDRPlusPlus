cmake_minimum_required(VERSION 3.13)
project(sdrpp_core)

if (USE_INTERNAL_LIBCORRECT)
    add_subdirectory("libcorrect/")
endif (USE_INTERNAL_LIBCORRECT)

if (USE_BUNDLE_DEFAULTS)
add_definitions(-DIS_MACOS_BUNDLE)
endif (USE_BUNDLE_DEFAULTS)

# Main code
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

add_definitions(-DSDRPP_IS_CORE)
if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

# Add code to dyn lib
add_library(sdrpp_core SHARED ${SRC})

# Set compiler options
if (MSVC)
    target_compile_options(sdrpp_core PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(sdrpp_core PRIVATE -O3 -std=c++17)
else ()
    target_compile_options(sdrpp_core PRIVATE -O3 -std=c++17)
endif ()

# Set the install prefix
target_compile_definitions(sdrpp_core PUBLIC INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

# Include core headers
target_include_directories(sdrpp_core PUBLIC "src/")
target_include_directories(sdrpp_core PUBLIC "src/imgui")

# Link to libcorrect
if (USE_INTERNAL_LIBCORRECT)
    target_include_directories(sdrpp_core PUBLIC "libcorrect/include")
    target_link_libraries(sdrpp_core PUBLIC correct_static)
endif (USE_INTERNAL_LIBCORRECT)

if (OPT_OVERRIDE_STD_FILESYSTEM)
    target_include_directories(sdrpp_core PUBLIC "std_replacement")
endif (OPT_OVERRIDE_STD_FILESYSTEM)

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")

    # Volk
    target_link_libraries(sdrpp_core PUBLIC volk)

    # OpenGL
    find_package(OpenGL REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC OpenGL::GL)

    # GLFW3
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC glfw)

    # FFTW3
    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC FFTW3::fftw3f)

    # WinSock2
    target_link_libraries(sdrpp_core PUBLIC wsock32 ws2_32)

else()
    find_package(PkgConfig)
    find_package(OpenGL REQUIRED)

    pkg_check_modules(FFTW3 REQUIRED fftw3f)
    pkg_check_modules(VOLK REQUIRED volk)
    pkg_check_modules(GLFW3 REQUIRED glfw3)

    target_include_directories(sdrpp_core PUBLIC
        ${OPENGL_INCLUDE_DIRS}
        ${FFTW3_INCLUDE_DIRS}
        ${GLFW3_INCLUDE_DIRS}
        ${VOLK_INCLUDE_DIRS}
    )
    
    target_link_directories(sdrpp_core PUBLIC
        ${OPENGL_LIBRARY_DIRS}
        ${FFTW3_LIBRARY_DIRS}
        ${GLFW3_LIBRARY_DIRS}
        ${VOLK_LIBRARY_DIRS}
    )

    target_link_libraries(sdrpp_core PUBLIC
        ${OPENGL_LIBRARIES}
        ${FFTW3_LIBRARIES}
        ${GLFW3_LIBRARIES}
        ${VOLK_LIBRARIES}
    )

    if (NOT USE_INTERNAL_LIBCORRECT)
        pkg_check_modules(CORRECT REQUIRED libcorrect)
        target_include_directories(sdrpp_core PUBLIC ${CORRECT_INCLUDE_DIRS})
        target_link_directories(sdrpp_core PUBLIC ${CORRECT_LIBRARY_DIRS})
        target_link_libraries(sdrpp_core PUBLIC ${CORRECT_LIBRARIES})
    endif (NOT USE_INTERNAL_LIBCORRECT)

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(sdrpp_core PUBLIC stdc++fs)
    endif ()

endif ()

set(CORE_FILES ${RUNTIME_OUTPUT_DIRECTORY} PARENT_SCOPE)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"

# Install directives
install(TARGETS sdrpp_core DESTINATION lib)