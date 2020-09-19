#include <module.h>
#include <signal_path/vfo_manager.h>
#include <gui/main_window.h>
#include <signal_path/audio.h>

namespace mod {
    API_t API;
    std::map<std::string, Module_t> modules;
    std::vector<std::string> moduleNames;
    ImGui::WaterFall* _wtf;

    std::string api_getSelectedVFOName() {
        return _wtf->selectedVFO;
    }

    void initAPI(ImGui::WaterFall* wtf) {
        _wtf = wtf;

        // GUI
        API.getSelectedVFOName = api_getSelectedVFOName;
        API.bindVolumeVariable = bindVolumeVariable;
        API.unbindVolumeVariable = unbindVolumeVariable;

        // Audio
        API.registerMonoStream = audio::registerMonoStream;
        API.registerStereoStream = audio::registerStereoStream;
        API.startStream = audio::startStream;
        API.stopStream = audio::stopStream;
        API.removeStream = audio::removeStream;
        API.bindToStreamMono = audio::bindToStreamMono;
        API.bindToStreamStereo = audio::bindToStreamStereo;
        API.setBlockSize = audio::setBlockSize;
        API.unbindFromStreamMono = audio::unbindFromStreamMono;
        API.unbindFromStreamStereo = audio::unbindFromStreamStereo;
        API.getStreamNameList = audio::getStreamNameList;
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
        mod._NEW_FRAME_ = (void(*)(void*))GetProcAddress(mod.inst, "_NEW_FRAME_");
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
        mod._NEW_FRAME_ = (void(*)(void*))dlsym(mod.inst, "_NEW_FRAME_");
        mod._DRAW_MENU_ = (void(*)(void*))dlsym(mod.inst, "_DRAW_MENU_");
        mod._HANDLE_EVENT_ = (void(*)(void*, int))dlsym(mod.inst, "_HANDLE_EVENT_");
        mod._STOP_ = (void(*)(void*))dlsym(mod.inst, "_STOP_");
#endif
        if (mod._INIT_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _INIT_.", name);
            return;
        }
        if (mod._NEW_FRAME_ == NULL) {
            spdlog::error("Couldn't load {0} because it's missing _NEW_FRAME_.", name);
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
        data << file;
        file.close();

        std::map<std::string, std::string> list = data.get<std::map<std::string, std::string>>();
        for (auto const& [name, file] : list) {
            spdlog::info("Loading {0} ({1})", name, file);
            loadModule(file, name);
        }
    }
};

