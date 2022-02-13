#pragma once
#include <thread>
#include <string>
#include <mutex>

namespace LoadingScreen {
    void init();
    void show(std::string msg);
};