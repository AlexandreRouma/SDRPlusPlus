#pragma once
#include <json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

using nlohmann::json;

namespace config {
    void load(std::string path);
    void startAutoSave();
    void stopAutoSave();
    void setRootDirectory(std::string dir);
    std::string getRootDirectory();

    extern bool configModified;
    extern json config;
};