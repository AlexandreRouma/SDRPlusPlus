include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)

include(nlohmann_json.cmake)
include(spdlog.cmake)
include(imgui.cmake)
include(stb.cmake)

if (OPT_OVERRIDE_STD_FILESYSTEM)
  add_library(ghc_filesystem INTERFACE)
  target_include_directories(ghc_filesystem INTERFACE std_replacement)
endif (OPT_OVERRIDE_STD_FILESYSTEM)
