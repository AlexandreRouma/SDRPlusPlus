FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG v1.83
  GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(imgui)

add_library(imgui
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(imgui
  PUBLIC
  glfw
  OpenGL::GL)
