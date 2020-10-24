#pragma once
#include <json.hpp>
#include <thread>
#include <string>
#include <mutex>

using nlohmann::json;

#define DEV_BUILD


#define SDRPP_RESOURCE_DIR      "/usr/local/"



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

    // static void setResourceDir(std::string path);
    // static std::string getResourceDir();

    // static void setConfigDir(std::string path);
    // static std::string getConfigDir();

    json conf;
    
private:
    static void autoSaveWorker(ConfigManager* _this);

    //static std::string resDir;
    //static std::string configDir;

    std::string path = "";
    bool changed = false;
    bool autoSaveEnabled = false;
    std::thread autoSaveThread;
    std::mutex mtx;

};