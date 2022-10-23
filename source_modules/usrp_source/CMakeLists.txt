cmake_minimum_required(VERSION 3.13)
project(usrp_source)

file(GLOB SRC "src/*.cpp")

add_library(usrp_source SHARED ${SRC})
target_link_libraries(usrp_source PRIVATE sdrpp_core)
set_target_properties(usrp_source PROPERTIES PREFIX "")

target_include_directories(usrp_source PRIVATE "src/")

if (MSVC)
    target_compile_options(usrp_source PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(usrp_source PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(usrp_source PRIVATE -O3 -std=c++17)
endif ()

if (MSVC)
    # Lib path
    target_link_directories(usrp_source PRIVATE "C:/Program Files/PothosSDR/bin/")

    target_include_directories(usrp_source PRIVATE "C:/Program Files/PothosSDR/include/")

    target_link_libraries(usrp_source PRIVATE uhd)
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBUHD REQUIRED uhd)

    target_include_directories(usrp_source PRIVATE ${LIBUHD_INCLUDE_DIRS})
    target_link_directories(usrp_source PRIVATE ${LIBUHD_LIBRARY_DIRS})
    target_link_libraries(usrp_source PRIVATE ${LIBUHD_LIBRARIES})
endif ()

# Install directives
install(TARGETS usrp_source DESTINATION lib/sdrpp/plugins)