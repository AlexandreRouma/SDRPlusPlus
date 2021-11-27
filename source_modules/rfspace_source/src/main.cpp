#include <rfspace_client.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <options.h>
#include <gui/widgets/stepped_slider.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "rfspace_source",
    /* Description:     */ "RFspace source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class SpyServerSourceModule : public ModuleManager::Instance {
public:
    SpyServerSourceModule(std::string name) {
        this->name = name;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        strcpy(hostname, "192.168.0.111");

        sigpath::sourceManager.registerSource("RFspace", &handler);
    }

    ~SpyServerSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("RFspace");
    }

    void postInit() {}

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
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        gui::mainWindow.playButtonLocked = !(_this->client && _this->client->isOpen());
        spdlog::info("SpyServerSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        gui::mainWindow.playButtonLocked = false;
        spdlog::info("SpyServerSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (_this->running) { return; }

        // TODO: Start

        _this->running = true;
        spdlog::info("SpyServerSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (!_this->running) { return; }
        
        // TODO: Stop

        _this->running = false;
        spdlog::info("SpyServerSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (_this->running) {
            // TODO: Tune
        }
        _this->freq = freq;
        spdlog::info("SpyServerSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        bool connected = (_this->client && _this->client->isOpen());
        gui::mainWindow.playButtonLocked = !connected;

        if (connected) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_rfspace_srv_host_", _this->name), _this->hostname, 1023)) {
            config.acquire();
            config.conf["hostname"] = _this->hostname;
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_rfspace_srv_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }
        if (connected) { style::endDisabled(); }

        if (_this->running) { style::beginDisabled(); }
        if (!connected && ImGui::Button("Connect##rfspace_source", ImVec2(menuWidth, 0))) {
            try {
                _this->client = rfspace::connect(_this->hostname, _this->port, &_this->stream);
            }
            catch (std::exception e) {
                spdlog::error("Could not connect to SDR: {0}", e.what());
            }
        }
        else if (connected && ImGui::Button("Disconnect##rfspace_source", ImVec2(menuWidth, 0))) {
            _this->client->close();
        }
        if (_this->running) { style::endDisabled(); }

        

        if (connected) {
            // TODO: Options here

            ImGui::Text("Status:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected (DEV NAME HERE)");
        }
        else {
            ImGui::Text("Status:");
            ImGui::SameLine();
            ImGui::Text("Not connected");
        }
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    double sampleRate = 1000000;
    double freq;

    char hostname[1024];
    int port = 50000;

    int srId = 0;
    std::vector<double> sampleRates;
    std::string sampleRatesTxt;

    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;

    rfspace::RFspaceClient client;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["hostname"] = "localhost";
    def["port"] = 50000;
    def["devices"] = json::object();
    config.setPath(options::opts.root + "/rfspace_config.json");
    config.load(def);
    config.enableAutoSave();

    // Check config in case a user has a very old version
    config.acquire();
    bool corrected = false;
    if (!config.conf.contains("hostname") || !config.conf.contains("port") || !config.conf.contains("devices")) {
        config.conf = def;
        corrected = true;
    }
    config.release(corrected);
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SpyServerSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (SpyServerSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}