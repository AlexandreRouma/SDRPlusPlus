#include <utils/networking.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <recorder_interface.h>
#include <meteor_demodulator_interface.h>
#include <config.h>
#include <cctype>
#include <radio_interface.h>
#define CONCAT(a, b) ((std::string(a) + b).c_str())

#define MAX_COMMAND_LENGTH 8192

SDRPP_MOD_INFO{
    /* Name:            */ "rigctl_client",
    /* Description:     */ "My fancy new module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

enum {
    RECORDER_TYPE_RECORDER,
    RECORDER_TYPE_METEOR_DEMODULATOR
};

ConfigManager config;

class SigctlServerModule : public ModuleManager::Instance {
public:
    SigctlServerModule(std::string name) {
        this->name = name;

        _retuneHandler.ctx = this;
        _retuneHandler.handler = retuneHandler;

        sigpath::sourceManager.onRetune.bindHandler(&_retuneHandler);
        gui::menu.registerEntry(name, menuHandler, this, NULL);
    }

    ~SigctlServerModule() {
        gui::menu.removeEntry(name);
        sigpath::sourceManager.onRetune.unbindHandler(&_retuneHandler);
    }

    void postInit() {
        
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        SigctlServerModule* _this = (SigctlServerModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (ImGui::Checkbox("Panadapter Mode", &_this->panMode)) {
            sigpath::sourceManager.setPanadpterIF(8830000.0);
            sigpath::sourceManager.setTuningMode(_this->panMode ? SourceManager::TuningMode::PANADAPTER : SourceManager::TuningMode::NORMAL);
        }
    }

    static void retuneHandler(double freq, void* ctx) {
        spdlog::warn("PAN RETUNE: {0}", freq);
    }

    std::string name;
    bool enabled = true;
    bool panMode = false;

    EventHandler<double> _retuneHandler;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/rigctl_client_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SigctlServerModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (SigctlServerModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
