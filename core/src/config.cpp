#include <config.h>
#include <spdlog/spdlog.h>
#include <fstream>

#include <filesystem>

ConfigManager::ConfigManager() {
    lastSave = std::chrono::system_clock::now();
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
        save();
    }
    if (!std::filesystem::is_regular_file(path)) {
        spdlog::error("Config file '{0}' isn't a file", path);
        return;
    }
    
    std::ifstream file(path.c_str());
    file >> conf;
    file.close();

    lastSave = std::chrono::system_clock::now();

    if (lock) { mtx.unlock(); }
}

void ConfigManager::save() {
    std::ofstream file(path.c_str());
    file << conf.dump(4);
    file.close();
}

void ConfigManager::enableAutoSave() {
    if (!autoSaveEnabled) { autoSaveEnabled = true; }
}

void ConfigManager::disableAutoSave() {
    if (autoSaveEnabled) { autoSaveEnabled = false; }
}

void ConfigManager::aquire() {
    mtx.lock();
}

void ConfigManager::release(bool changed) {
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSave);

    if (changed && autoSaveEnabled && ms.count() >= 1000) {
        save();

        // need to call now() again in case save has been very slow
        lastSave = std::chrono::system_clock::now();
    }
    mtx.unlock();
}