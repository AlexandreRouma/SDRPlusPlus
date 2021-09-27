cmake_minimum_required(VERSION 3.13)
project(m17_decoder)

add_subdirectory("libcorrect")

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include_directories("src/")
include_directories("libcorrect/include")

include_directories("C:/Users/ryzerth/Documents/Code/codec2/src")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

add_library(m17_decoder SHARED ${SRC})
target_link_libraries(m17_decoder PRIVATE sdrpp_core)
target_link_libraries(m17_decoder PRIVATE correct_static)
set_target_properties(m17_decoder PROPERTIES PREFIX "")

target_link_directories(sdrpp_core PUBLIC "C:/Users/ryzerth/Documents/Code/codec2/build/src")
target_link_libraries(m17_decoder PRIVATE libcodec2)

# Install directives
install(TARGETS m17_decoder DESTINATION lib/sdrpp/plugins)