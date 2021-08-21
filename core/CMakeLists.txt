# Set compiler options
if (MSVC)
    set(CMAKE_CXX_FLAGS "/EHsc")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-Wno-unused-command-line-argument -undefined dynamic_lookup")
endif ()

# Main code
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

add_library(sdrpp_core SHARED ${SRC})
target_compile_features(sdrpp_core PUBLIC cxx_std_17)
target_compile_definitions(sdrpp_core PUBLIC
  SDRPP_IS_CORE
  INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")
target_include_directories(sdrpp_core PUBLIC src)

find_package(Volk REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

# https://github.com/FFTW/fftw3/issues/130
find_package(PkgConfig REQUIRED)
pkg_search_module(FFTW3 REQUIRED fftw3f IMPORTED_TARGET)

target_link_libraries(sdrpp_core
  PUBLIC
    Volk::volk
    GLEW::glew
    glfw
    PkgConfig::FFTW3
    imgui
    stb
    spdlog
    nlohmann_json
    $<$<BOOL:OPT_OVERRIDE_STD_FILESYSTEM>:ghc_filesystem>
)

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")

    # WinSock2
    target_link_libraries(sdrpp_core PUBLIC wsock32 ws2_32)
else()
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(sdrpp_core PUBLIC stdc++fs)
    endif ()

    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif ()
endif ()

install(TARGETS sdrpp_core DESTINATION lib)
