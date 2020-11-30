#pragma once
#include <thread>
#include <string>
#include <mutex>
#include <GLFW/glfw3.h>

namespace LoadingScreen {
    void setWindow(GLFWwindow* win);
    void show(std::string msg);
};