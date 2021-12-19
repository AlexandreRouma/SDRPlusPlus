#pragma once
#include <json.hpp>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>

using nlohmann::json;

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    void setPath(std::string file);
    void load(json def, bool lock = true);
    void save(bool lock = true);
    void enableAutoSave();
    void disableAutoSave();
    void acquire();
    void release(bool modified = false);

    json conf;

private:
    void autoSaveWorker();

    std::string path = "";
    volatile bool changed = false;
    volatile bool autoSaveEnabled = false;
    std::thread autoSaveThread;
    std::mutex mtx;

    std::mutex termMtx;
    std::condition_variable termCond;
    volatile bool termFlag = false;
};