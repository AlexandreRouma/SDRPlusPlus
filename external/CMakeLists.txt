include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG v3.10.0
)

FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog
  GIT_TAG        v1.x
)

FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG v1.83
)

FetchContent_MakeAvailable(imgui)

add_library(imgui
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(imgui
  PUBLIC
    glfw
    OpenGL::GL)

FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb)

FetchContent_MakeAvailable(stb)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
