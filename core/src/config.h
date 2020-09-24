#pragma once
#include <json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <string>

using nlohmann::json;

#define DEV_BUILD

#ifndef ROOT_DIR
#ifdef DEV_BUILD
#define ROOT_DIR    "../root_dev"
#elif _WIN32
#define ROOT_DIR    "."
#else
#define ROOT_DIR    "/etc/sdrpp"
#endif
#endif

class ConfigManager {
public:
    ConfigManager();
    void setPath(std::string file);
    void load(json def, bool lock = true);
    void save(bool lock = true);
    void enableAutoSave();
    void disableAutoSave();
    void aquire();
    void release(bool changed = false);

    json conf;
    
private:
    static void autoSaveWorker(ConfigManager* _this);

    std::string path = "";
    bool changed = false;
    bool autoSaveEnabled = false;
    std::thread autoSaveThread;
    std::mutex mtx;

};