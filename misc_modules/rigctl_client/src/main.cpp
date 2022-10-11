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

        strcpy(host, "127.0.0.1");

        _retuneHandler.ctx = this;
        _retuneHandler.handler = retuneHandler;

        gui::menu.registerEntry(name, menuHandler, this, NULL);
    }

    ~SigctlServerModule() {
        stop();
        gui::menu.removeEntry(name);
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

    void start() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (running) { return; }

        sigpath::sourceManager.setPanadpterIF(ifFreq);
        sigpath::sourceManager.setTuningMode(SourceManager::TuningMode::PANADAPTER);
        sigpath::sourceManager.onRetune.bindHandler(&_retuneHandler);
        running = true;
    }

    void stop() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (!running) { return; }

        sigpath::sourceManager.onRetune.unbindHandler(&_retuneHandler);
        sigpath::sourceManager.setTuningMode(SourceManager::TuningMode::NORMAL);
        running = false;
    }

private:
    static void menuHandler(void* ctx) {
        SigctlServerModule* _this = (SigctlServerModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (_this->running) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_rigctl_cli_host_", _this->name), _this->host, 1023)) {
            config.acquire();
            config.conf[_this->name]["host"] = std::string(_this->host);
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_rigctl_cli_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf[_this->name]["port"] = _this->port;
            config.release(true);
        }
        if (_this->running) { style::endDisabled(); }

        ImGui::LeftLabel("IF Frequency");
        ImGui::FillWidth();
        if (ImGui::InputDouble(CONCAT("##_rigctl_if_freq_", _this->name), &_this->ifFreq, 100.0, 100000.0, "%.0f")) {
            if (_this->running) {
                sigpath::sourceManager.setPanadpterIF(_this->ifFreq);
            }
            config.acquire();
            config.conf[_this->name]["ifFreq"] = _this->ifFreq;
            config.release(true);
        }

        ImGui::FillWidth();
        if (_this->running && ImGui::Button(CONCAT("Stop##_rigctl_cli_stop_", _this->name), ImVec2(menuWidth, 0))) {
            _this->stop();
        }
        else if (!_this->running && ImGui::Button(CONCAT("Start##_rigctl_cli_stop_", _this->name), ImVec2(menuWidth, 0))) {
            _this->start();
        }
    }

    static void retuneHandler(double freq, void* ctx) {
        spdlog::warn("PAN RETUNE: {0}", freq);
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    std::recursive_mutex mtx;

    char host[1024];
    int port = 4532;

    double ifFreq = 8830000.0;

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
