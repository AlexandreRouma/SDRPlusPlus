#include <module.h>
#include <signal_path/vfo_manager.h>
#include <gui/main_window.h>
#include <signal_path/audio.h>

namespace mod {
    std::map<std::string, Module_t> modules;
    std::vector<std::string> moduleNames;
    ImGui::WaterFall* _wtf;

    void loadModule(std::string path, std::string name) {
        if (!std::filesystem::exists(path)) {
            spdlog::error("{0} does not exist", path);
            return;
        }
        if (!std::filesystem::is_regular_file(path)) {
            spdlog::error("{0} isn't a loadable module", path);
            return;
        }
        Module_t mod;
#ifdef _WIN32
        mod.inst = LoadLibraryA(path.c_str());
        if (mod.inst == NULL) {
            spdlog::error("Couldn't load {0}.", name);
            return;
        }

        mod._INIT_ = (void(*)())GetProcAddress(mod.inst, "_INIT_");
        mod._CREATE_INSTANCE_ = (void*(*)(std::string))GetProcAddress(mod.inst, "_CREATE_INSTANCE_");
        mod._DELETE_INSTANCE_ = (void(*)(void*))GetProcAddress(mod.inst, "_DELETE_INSTANCE_");
        mod._STOP_ = (void(*)(void*))GetProcAddress(mod.inst, "_STOP_");
#else
        mod.inst = dlopen(path.c_str(), RTLD_LAZY);
        if (mod.inst == NULL) {
            spdlog::error("Couldn't load {0}.", name);
            return;
        }
        mod._INIT_ = (void(*)())dlsym(mod.inst, "_INIT_");
        mod._CREATE_INSTANCE_ = (void*(*)(std::string))dlsym(mod.inst, "_CREATE_INSTANCE_");
        mod._DELETE_INSTANCE_ = (void(*)(void*))dlsym(mod.inst, "_DELETE_INSTANCE_");
        mod._STOP_ = (void(*)())dlsym(mod.inst, "_STOP_");
#endif
        if (mod._INIT_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _INIT_.", name);
            return;
        }
        if (mod._CREATE_INSTANCE_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _CREATE_INSTANCE_.", name);
            return;
        }
        if (mod._DELETE_INSTANCE_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _DELETE_INSTANCE_.", name);
            return;
        }
        if (mod._STOP_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _STOP_.", name);
            return;
        }

        if (!isLoaded(mod.inst)) {
            mod._INIT_();
        }
        
        mod.ctx = mod._CREATE_INSTANCE_(name);   
        if (mod.ctx == NULL) {
            spdlog::error("{0} Failed to initialize.", name);
        }

        modules[name] = mod;
        moduleNames.push_back(name);
    }

    void loadFromList(std::string path) {
        if (!std::filesystem::exists(path)) {
            spdlog::error("Module list file does not exist");
            return;
        }
        if (!std::filesystem::is_regular_file(path)) {
            spdlog::error("Module list file isn't a file...");
            return;
        }
        std::ifstream file(path.c_str());
        json data;
        file >> data;
        file.close();

        std::map<std::string, std::string> list = data.get<std::map<std::string, std::string>>();
        for (auto const& [name, file] : list) {
            spdlog::info("Loading {0} ({1})", name, file);
            loadModule(file, name);
        }
    }

    bool isLoaded(void* handle) {
        for (auto const& [name, module] : modules) {
            if (module.inst == handle) {
                return true;
            }
        }
        return false;
    }
};

