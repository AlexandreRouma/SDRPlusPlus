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
    
    std::ifstream file(path.c_str());
    file >> conf;
    file.close();
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
    if (!autoSaveEnabled) {
        autoSaveEnabled = true;
        termFlag = false;
        autoSaveThread = std::thread(autoSaveWorker, this);
    }
}

void ConfigManager::disableAutoSave() {
    if (autoSaveEnabled) {
        {
            std::lock_guard<std::mutex> lock(termMtx);
            autoSaveEnabled = false;
            termFlag = true;
        }
        termCond.notify_one();
        if (autoSaveThread.joinable()) { autoSaveThread.join(); }
    }
}

void ConfigManager::acquire() {
    mtx.lock();
}

void ConfigManager::release(bool changed) {
    this->changed |= changed;
    mtx.unlock();
}

void ConfigManager::autoSaveWorker(ConfigManager* _this) {
    while (_this->autoSaveEnabled) {
        if (!_this->mtx.try_lock()) {
            spdlog::warn("ConfigManager locked, waiting...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        if (_this->changed) {
            _this->changed = false;
            _this->save(false);
        }
        _this->mtx.unlock();

        // Sleep but listen for wakeup call
        {
            std::unique_lock<std::mutex> lock(_this->termMtx);
            _this->termCond.wait_for(lock, std::chrono::milliseconds(1000), [_this]() { return _this->termFlag; } );
        }
    }
}