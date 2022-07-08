#include "sdrpp_server_client.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/widgets/stepped_slider.h>
#include <utils/optionlist.h>
#include <gui/dialogs/dialog_box.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "sdrpp_server_source",
    /* Description:     */ "SDR++ Server source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class SDRPPServerSourceModule : public ModuleManager::Instance {
public:
    SDRPPServerSourceModule(std::string name) {
        this->name = name;

        // Yeah no server-ception, sorry...
        if (core::args["server"].b()) { return; }

        // Initialize lists
        sampleTypeList.define("Int8", dsp::compression::PCM_TYPE_I8);
        sampleTypeList.define("Int16", dsp::compression::PCM_TYPE_I16);
        sampleTypeList.define("Float32", dsp::compression::PCM_TYPE_F32);
        sampleTypeId = sampleTypeList.valueId(dsp::compression::PCM_TYPE_I16);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        // Load config
        config.acquire();
        std::string hostStr = config.conf["hostname"];
        strcpy(hostname, hostStr.c_str());
        port = config.conf["port"];
        config.release();

        sigpath::sourceManager.registerSource("SDR++ Server", &handler);
    }

    ~SDRPPServerSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("SDR++ Server");
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
        SDRPPServerSourceModule* _this = (SDRPPServerSourceModule*)ctx;
        if (_this->client) {
            core::setInputSampleRate(_this->client->getSampleRate());
        }
        gui::mainWindow.playButtonLocked = !(_this->client && _this->client->isOpen());
        spdlog::info("SDRPPServerSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SDRPPServerSourceModule* _this = (SDRPPServerSourceModule*)ctx;
        gui::mainWindow.playButtonLocked = false;
        spdlog::info("SDRPPServerSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        SDRPPServerSourceModule* _this = (SDRPPServerSourceModule*)ctx;
        if (_this->running) { return; }

        // TODO: Set configuration here
        if (_this->client) {
            _this->client->setFrequency(_this->freq);
            _this->client->start();
        }

        _this->running = true;
        spdlog::info("SDRPPServerSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        SDRPPServerSourceModule* _this = (SDRPPServerSourceModule*)ctx;
        if (!_this->running) { return; }

        if (_this->client) { _this->client->stop(); }

        _this->running = false;
        spdlog::info("SDRPPServerSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        SDRPPServerSourceModule* _this = (SDRPPServerSourceModule*)ctx;
        if (_this->running && _this->client) {
            _this->client->setFrequency(freq);
        }
        _this->freq = freq;
        spdlog::info("SDRPPServerSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        SDRPPServerSourceModule* _this = (SDRPPServerSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        bool connected = (_this->client && _this->client->isOpen());
        gui::mainWindow.playButtonLocked = !connected;

        ImGui::GenericDialog("##sdrpp_srv_src_err_dialog", _this->serverBusy, GENERIC_DIALOG_BUTTONS_OK, [=](){
            ImGui::TextUnformatted("This server is already in use.");
        });

        if (connected) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##sdrpp_srv_srv_host_", _this->name), _this->hostname, 1023)) {
            config.acquire();
            config.conf["hostname"] = _this->hostname;
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##sdrpp_srv_srv_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }
        if (connected) { style::endDisabled(); }

        if (_this->running) { style::beginDisabled(); }
        if (!connected && ImGui::Button("Connect##sdrpp_srv_source", ImVec2(menuWidth, 0))) {
            try {
                if (_this->client) { _this->client.reset(); }
                _this->client = server::connect(_this->hostname, _this->port, &_this->stream);
                _this->deviceInit();
            }
            catch (std::exception e) {
                spdlog::error("Could not connect to SDR: {0}", e.what());
                if (!strcmp(e.what(), "Server busy")) { _this->serverBusy = true; }
            }
        }
        else if (connected && ImGui::Button("Disconnect##sdrpp_srv_source", ImVec2(menuWidth, 0))) {
            _this->client->close();
        }
        if (_this->running) { style::endDisabled(); }


        if (connected) {
            ImGui::LeftLabel("Sample type");
            ImGui::FillWidth();
            if (ImGui::Combo("##sdrpp_srv_source_samp_type", &_this->sampleTypeId, _this->sampleTypeList.txt)) {
                _this->client->setSampleType(_this->sampleTypeList[_this->sampleTypeId]);

                // Save config
                config.acquire();
                config.conf["servers"][_this->devConfName]["sampleType"] = _this->sampleTypeList.key(_this->sampleTypeId);
                config.release(true);
            }
            
            if (ImGui::Checkbox("Compression", &_this->compression)) {
                _this->client->setCompression(_this->compression);

                // Save config
                config.acquire();
                config.conf["servers"][_this->devConfName]["compression"] = _this->compression;
                config.release(true);
            }

            bool dummy = true;
            style::beginDisabled();
            ImGui::Checkbox("Full IQ", &dummy);
            style::endDisabled();

            // Calculate datarate
            _this->frametimeCounter += ImGui::GetIO().DeltaTime;
            if (_this->frametimeCounter >= 0.2f) {
                _this->datarate = ((float)_this->client->bytes / (_this->frametimeCounter * 1024.0f * 1024.0f)) * 8;
                _this->frametimeCounter = 0;
                _this->client->bytes = 0;
            }

            ImGui::TextUnformatted("Status:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected (%.3f Mbit/s)", _this->datarate);

            ImGui::CollapsingHeader("Source [REMOTE]", ImGuiTreeNodeFlags_DefaultOpen);

            _this->client->showMenu();
        }
        else {
            ImGui::TextUnformatted("Status:");
            ImGui::SameLine();
            ImGui::TextUnformatted("Not connected (--.--- Mbit/s)");
        }
    }

    void deviceInit() {
        // Generate the config name
        char buf[4096];
        sprintf(buf, "%s:%05d", hostname, port);
        devConfName = buf;

        // Load settings
        sampleTypeId = sampleTypeList.valueId(dsp::compression::PCM_TYPE_I16);
        if (config.conf["servers"][devConfName].contains("sampleType")) {
            std::string key = config.conf["servers"][devConfName]["sampleType"];
            if (sampleTypeList.keyExists(key)) { sampleTypeId = sampleTypeList.keyId(key); }
        }
        if (config.conf["servers"][devConfName].contains("compression")) {
            compression = config.conf["servers"][devConfName]["compression"];
        }

        // Set settings
        client->setSampleType(sampleTypeList[sampleTypeId]);
        client->setCompression(compression);
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    
    double freq;
    bool serverBusy = false;

    float datarate = 0;
    float frametimeCounter = 0;

    char hostname[1024];
    int port = 50000;
    std::string devConfName = "";

    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;

    OptionList<std::string, dsp::compression::PCMType> sampleTypeList;
    int sampleTypeId;
    bool compression = false;

    server::Client client;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["hostname"] = "localhost";
    def["port"] = 5259;
    def["servers"] = json::object();
    config.setPath(core::args["root"].s() + "/sdrpp_server_source_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SDRPPServerSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (SDRPPServerSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}