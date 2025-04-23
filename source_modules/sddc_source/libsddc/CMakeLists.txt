cmake_minimum_required(VERSION 3.13)
project(libsddc VERSION 0.2.0)

# Options
option(BUILD_SDDC_UTILS "Build SDDC utilities such as sddc_info" ON)
option(INSTALL_UDEV_RULES "Install UDEV rules (Linux only)" ON)

# List all source files
file(GLOB_RECURSE SRC "src/*.c")

# On windows, all symbols must be exported
if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

# Create dynamic libs
add_library(sddc SHARED ${SRC})

# # Set optimisation flags
# if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
#     # Debug Flags
#     if (MSVC)
#         target_compile_options(sddc PRIVATE /EHsc)
#     else ()
#         target_compile_options(sddc PRIVATE -g -Og)
#     endif ()
# else()
#     # Normal Flags
#     if (MSVC)
#         target_compile_options(sddc PRIVATE /O2 /Ob2 /EHsc)
#     else ()
#         target_compile_options(sddc PRIVATE -O3)
#     endif ()
# endif()

# Include the include folder
target_include_directories(sddc PUBLIC "include/")

# Find libusb
find_package(PkgConfig REQUIRED)
pkg_check_modules(libusb REQUIRED IMPORTED_TARGET libusb-1.0)

# Link to libusb
target_link_libraries(sddc PRIVATE PkgConfig::libusb)

# TODO: Have it default instead of override
if (MSVC)
    set(CMAKE_INSTALL_PREFIX "C:/Program Files/SDDC/")
    set(CMAKE_INSTALL_BINDIR "bin")
    set(CMAKE_INSTALL_LIBDIR "lib")
    set(CMAKE_INSTALL_INCLUDEDIR "include")
else ()
    include(GNUInstallDirs)
endif ()

if (NOT MSVC)
    # Configure pkgconfig file
    configure_file(${CMAKE_SOURCE_DIR}/libsddc.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libsddc.pc @ONLY)

    # Install pkgconfig file
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libsddc.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif ()

# Install the library
if (MSVC)
    install(TARGETS sddc)
else ()
    install(TARGETS sddc DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif ()

# Install the headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# On Windows, install dependencies
if (MSVC)
    install(FILES $<TARGET_FILE_DIR:sddc>/libusb-1.0.dll DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

# Build utils if enabled
if (BUILD_SDDC_UTILS)
    add_subdirectory("utils/sddc_info")
    add_subdirectory("utils/sddc_rx")
endif ()

# # Create uninstall target
# configure_file(${CMAKE_SOURCE_DIR}/cmake/uninstall.cmake ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake @ONLY)
# add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)