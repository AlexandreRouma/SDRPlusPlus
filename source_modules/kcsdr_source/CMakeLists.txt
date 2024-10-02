cmake_minimum_required(VERSION 3.13)
project(kcsdr_source)

file(GLOB SRC "src/*.cpp" "src/*.c")

include(${SDRPP_MODULE_CMAKE})

target_link_directories(kcsdr_source PRIVATE "vendor/FTD3XXLibrary_1.3.0.10/x64/DLL")
target_include_directories(kcsdr_source PRIVATE "vendor/FTD3XXLibrary_1.3.0.10")
target_link_libraries(kcsdr_source PRIVATE FTD3XX)