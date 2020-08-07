#include <module.h>

namespace mod {
    API_t API;
    std::map<std::string, Module_t> modules;
    std::vector<std::string> moduleNames;

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
        mod._INIT_ = (void*(*)(mod::API_t*,ImGuiContext*,std::string))GetProcAddress(mod.inst, "_INIT_"); 
        mod._DRAW_MENU_ = (void(*)(void*))GetProcAddress(mod.inst, "_DRAW_MENU_");
        mod._STOP_ = (void(*)(void*))GetProcAddress(mod.inst, "_STOP_");
#else
        // Linux function here
#endif
        if (mod._INIT_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _INIT_.", name);
            return;
        }
        if (mod._DRAW_MENU_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _DRAW_MENU_.", name);
            return;
        }
        if (mod._STOP_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _STOP_.", name);
            return;
        }
        mod.ctx = mod._INIT_(&API, ImGui::GetCurrentContext(), name);
        if (mod.ctx == NULL) {
            spdlog::error("{0} Failed to initialize.", name);
        }
        modules[name] = mod;
        moduleNames.push_back(name);
    }
};

