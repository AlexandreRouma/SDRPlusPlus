cmake_minimum_required(VERSION 3.13)

if (MSVC)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif (MSVC)

include_directories("." "../Core")

add_library(sddc SHARED
    libsddc.cpp
)

if (MSVC)
  target_link_libraries(sddc PUBLIC Setupapi.lib)
else()
  target_include_directories(sddc PUBLIC "${LIBUSB_INCLUDE_DIR}")
  target_link_directories(sddc PUBLIC "${LIBUSB_LIBRARY_DIRS}")
  target_link_libraries(sddc PUBLIC ${LIBUSB_LIBRARIES})
endif (MSVC)

target_include_directories(sddc PUBLIC "${LIBFFTW_INCLUDE_DIR}")
target_link_directories(sddc PUBLIC "${LIBFFTW_LIBRARY_DIRS}")
target_link_libraries(sddc PUBLIC ${LIBFFTW_LIBRARIES})

target_link_libraries(sddc PRIVATE SDDC_CORE)
set_target_properties(sddc PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(sddc PROPERTIES SOVERSION 0)

# applications
add_executable(sddc_test sddc_test.c)
target_link_libraries(sddc_test PRIVATE sddc ${ASANLIB})

add_executable(sddc_stream_test sddc_stream_test.c wavewrite.c)
target_link_libraries(sddc_stream_test sddc ${ASANLIB})

add_executable(sddc_vhf_stream_test sddc_vhf_stream_test.c wavewrite.c)
target_link_libraries(sddc_vhf_stream_test sddc ${ASANLIB})
