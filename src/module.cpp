#include <module.h>
#include <vfo_manager.h>

namespace mod {
    API_t API;
    std::map<std::string, Module_t> modules;
    std::vector<std::string> moduleNames;

    void initAPI() {
        API.registerVFO = vfoman::create;
        API.setVFOOffset = vfoman::setOffset;
        API.setVFOCenterOffset = vfoman::setCenterOffset;
        API.setVFOBandwidth = vfoman::setBandwidth;
        API.setVFOSampleRate = vfoman::setSampleRate;
        API.getVFOOutputBlockSize = vfoman::getOutputBlockSize;
        API.setVFOReference = vfoman::setReference;
        API.removeVFO = vfoman::remove;
    }

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
        mod._HANDLE_EVENT_ = (void(*)(void*, int))GetProcAddress(mod.inst, "_HANDLE_EVENT_");
        mod._STOP_ = (void(*)(void*))GetProcAddress(mod.inst, "_STOP_");
#else
        mod.inst = dlopen(path.c_str(), RTLD_LAZY);
        if (mod.inst == NULL) {
            spdlog::error("Couldn't load {0}.", name);
            return;
        }
        mod._INIT_ = (void*(*)(mod::API_t*,ImGuiContext*,std::string))dlsym(mod.inst, "_INIT_"); 
        mod._DRAW_MENU_ = (void(*)(void*))dlsym(mod.inst, "_DRAW_MENU_");
        mod._HANDLE_EVENT_ = (void(*)(void*, int))dlsym(mod.inst, "_HANDLE_EVENT_");
        mod._STOP_ = (void(*)(void*))dlsym(mod.inst, "_STOP_");
#endif
        if (mod._INIT_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _INIT_.", name);
            return;
        }
        if (mod._DRAW_MENU_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _DRAW_MENU_.", name);
            return;
        }
        if (mod._HANDLE_EVENT_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _HANDLE_EVENT_.", name);
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

    void broadcastEvent(int eventId) {
        if (eventId < 0 || eventId >= _EVENT_COUNT) {
            return;
        }
        for (auto const& [name, mod] : modules) {
            mod._HANDLE_EVENT_(mod.ctx, eventId);
        }
    }
};

