#include <config.h>

namespace config {
    bool configModified = false;
    json config;
    bool autoSaveRunning = false;
    std::string _path;
    std::thread _workerThread;
    std::string rootDir;

    void _autoSaveWorker() {
        while (autoSaveRunning) {
            if (configModified) {
                configModified = false;
                std::ofstream file(_path.c_str());
                file << std::setw(4) << config;
                file.close();
                spdlog::info("Config saved");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    void load(std::string path) {
        if (!std::filesystem::exists(path)) {
            spdlog::error("Config file does not exist");
            return;
        }
        if (!std::filesystem::is_regular_file(path)) {
            spdlog::error("Config file isn't a file...");
            return;
        }
        _path = path;
        std::ifstream file(path.c_str());
        config << file;
        file.close();
    }

    void startAutoSave() {
        if (autoSaveRunning) {
            return;
        }
        autoSaveRunning = true;
        _workerThread = std::thread(_autoSaveWorker);
    }

    void stopAutoSave() {
        if (!autoSaveRunning) {
            return;
        }
        autoSaveRunning = false;
        _workerThread.join();
    }

    void setRootDirectory(std::string dir) {
        rootDir = dir;
    }

    std::string getRootDirectory() {
        return rootDir;
    }
};