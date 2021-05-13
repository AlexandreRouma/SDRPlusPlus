cmake_minimum_required(VERSION 3.13)
project(SDDC VERSION 1.0.1)

include(CTest)

### build options
# default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE} " - Version: " ${VERSION} " / " ${LIBVER})

# allow disabling optimizations - for debug reasons
option(USE_SIMD_OPTIMIZATIONS "enable SIMD optimizations" ON)

# allow enabling address sanitizer - for debug reasons
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    option(USE_DEBUG_ASAN "use GCC's address sanitizer?" OFF)
endif()

if (USE_DEBUG_ASAN)
  set(ASANLIB "asan")
else()
  set(ASANLIB "")
endif()

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3")

    get_filename_component(SDKPATH ${CMAKE_LINKER} DIRECTORY)
    find_program(LIBEXE lib HINTS ${SDKPATH} REQUIRED)

    # External Project FFTW on Windows
    if(${CMAKE_EXE_LINKER_FLAGS} MATCHES "X86")
        SET(FFTW_URL "ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll32.zip")
        SET(ARCH x86)
        SET(HASH 29882a43033c9393479a4df52a2e9120589c06a2b724155b1a682747fa3e57d4)
    else()
        SET(FFTW_URL "ftp://ftp.fftw.org/pub/fftw/fftw-3.3.5-dll64.zip")
        SET(ARCH x64)
        SET(HASH cfd88dc0e8d7001115ea79e069a2c695d52c8947f5b4f3b7ac54a192756f439f)
    endif()

    include(ExternalProject)
    ExternalProject_Add(
        LIBFFTW
        URL ${FFTW_URL}
        URL_HASH SHA256=${HASH}
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ${LIBEXE} /def:./libfftw3f-3.def /MACHINE:${ARCH} /OUT:./fftw3f-3.lib
        INSTALL_COMMAND     ""
    )
    ExternalProject_Get_Property(LIBFFTW SOURCE_DIR)
    SET(LIBFFTW_INCLUDE_DIRS ${SOURCE_DIR})
    SET(LIBFFTW_LIBRARY_DIRS ${SOURCE_DIR})
    SET(LIBFFTW_LIBRARIES fftw3f-3)

    add_subdirectory(ExtIO_sddc)
else()

    if (USE_DEBUG_ASAN)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        set(CMAKE_CXX_FLAGS "-O0 -std=c++17 -Wall -Werror")
    else()
        set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wall -Werror")
    endif(USE_DEBUG_ASAN)
    #add_compile_options(-Wall -Wextra -pedantic)
    include(FindPkgConfig)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    pkg_check_modules(LIBFFTW REQUIRED fftw3f)
endif (MSVC)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

add_subdirectory(Core)
add_subdirectory(libsddc)
add_subdirectory(unittest)
