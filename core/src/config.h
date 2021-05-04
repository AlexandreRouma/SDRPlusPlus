#pragma once
#include <json.hpp>
#include <atomic>
#include <string>
#include <mutex>

using nlohmann::json;

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    void setPath(std::string file);
    void load(json def, bool lock = true);
    void save();
    void enableAutoSave();
    void disableAutoSave();
    void aquire();
    void release(bool changed = false);

    json conf;
    
private:
    std::string path = "";
    std::atomic<bool> autoSaveEnabled = false;
    std::chrono::time_point<std::chrono::system_clock> lastSave;
    std::mutex mtx;
};