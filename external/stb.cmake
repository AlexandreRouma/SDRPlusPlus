FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb
  GIT_PROGRESS TRUE)

FetchContent_GetProperties(stb)
if (NOT stb_POPULATED)
  FetchContent_Populate(stb)

  add_library(stb INTERFACE)
  target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()
