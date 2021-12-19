#include <config.h>
#include <spdlog/spdlog.h>
#include <fstream>

#include <filesystem>

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
    disableAutoSave();
}

void ConfigManager::setPath(std::string file) {
    path = file;
}

void ConfigManager::load(json def, bool lock) {
    if (lock) { mtx.lock(); }
    if (path == "") {
        spdlog::error("Config manager tried to load file with no path specified");
        return;
    }
    if (!std::filesystem::exists(path)) {
        spdlog::warn("Config file '{0}' does not exist, creating it", path);
        conf = def;
        save(false);
    }
    if (!std::filesystem::is_regular_file(path)) {
        spdlog::error("Config file '{0}' isn't a file", path);
        return;
    }

    try {
        std::ifstream file(path.c_str());
        file >> conf;
        file.close();
    }
    catch (std::exception e) {
        spdlog::error("Config file '{0}' is corrupted, resetting it", path);
        conf = def;
        save(false);
    }
    if (lock) { mtx.unlock(); }
}

void ConfigManager::save(bool lock) {
    if (lock) { mtx.lock(); }
    std::ofstream file(path.c_str());
    file << conf.dump(4);
    file.close();
    if (lock) { mtx.unlock(); }
}

void ConfigManager::enableAutoSave() {
    if (autoSaveEnabled) { return; }
    autoSaveEnabled = true;
    termFlag = false;
    autoSaveThread = std::thread(&ConfigManager::autoSaveWorker, this);
}

void ConfigManager::disableAutoSave() {
    if (!autoSaveEnabled) { return; }
    {
        std::lock_guard<std::mutex> lock(termMtx);
        autoSaveEnabled = false;
        termFlag = true;
    }
    termCond.notify_one();
    if (autoSaveThread.joinable()) { autoSaveThread.join(); }
}

void ConfigManager::acquire() {
    mtx.lock();
}

void ConfigManager::release(bool modified) {
    changed |= modified;
    mtx.unlock();
}

void ConfigManager::autoSaveWorker() {
    while (autoSaveEnabled) {
        if (!mtx.try_lock()) {
            spdlog::warn("ConfigManager locked, waiting...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        if (changed) {
            changed = false;
            save(false);
        }
        mtx.unlock();

        // Sleep but listen for wakeup call
        {
            std::unique_lock<std::mutex> lock(termMtx);
            termCond.wait_for(lock, std::chrono::milliseconds(1000), [this]() { return termFlag; });
        }
    }
}