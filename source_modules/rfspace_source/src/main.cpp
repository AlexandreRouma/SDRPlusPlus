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
#include <utils/optionlist.h>
#include <gui/smgui.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "rfspace_source",
    /* Description:     */ "RFspace source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class RFSpaceSourceModule : public ModuleManager::Instance {
public:
    RFSpaceSourceModule(std::string name) {
        this->name = name;

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

        sigpath::sourceManager.registerSource("RFspace", &handler);
    }

    ~RFSpaceSourceModule() {
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
        RFSpaceSourceModule* _this = (RFSpaceSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        gui::mainWindow.playButtonLocked = !(_this->client && _this->client->isOpen());
        spdlog::info("RFSpaceSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RFSpaceSourceModule* _this = (RFSpaceSourceModule*)ctx;
        gui::mainWindow.playButtonLocked = false;
        spdlog::info("RFSpaceSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        RFSpaceSourceModule* _this = (RFSpaceSourceModule*)ctx;
        if (_this->running) { return; }

        // TODO: Set configuration here
        if (_this->client) { _this->client->start(rfspace::RFSPACE_SAMP_FORMAT_COMPLEX, rfspace::RFSPACE_SAMP_FORMAT_16BIT); }

        _this->running = true;
        spdlog::info("RFSpaceSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RFSpaceSourceModule* _this = (RFSpaceSourceModule*)ctx;
        if (!_this->running) { return; }

        if (_this->client) { _this->client->stop(); }

        _this->running = false;
        spdlog::info("RFSpaceSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RFSpaceSourceModule* _this = (RFSpaceSourceModule*)ctx;
        if (_this->running && _this->client) {
            _this->client->setFrequency(freq);
        }
        _this->freq = freq;
        spdlog::info("RFSpaceSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        RFSpaceSourceModule* _this = (RFSpaceSourceModule*)ctx;

        bool connected = (_this->client && _this->client->isOpen());
        gui::mainWindow.playButtonLocked = !connected;

        if (connected) { SmGui::BeginDisabled(); }
        if (SmGui::InputText(CONCAT("##_rfspace_srv_host_", _this->name), _this->hostname, 1023)) {
            config.acquire();
            config.conf["hostname"] = _this->hostname;
            config.release(true);
        }
        SmGui::SameLine();
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_rfspace_srv_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }
        if (connected) { SmGui::EndDisabled(); }

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (!connected && SmGui::Button("Connect##rfspace_source")) {
            try {
                if (_this->client) { _this->client.reset(); }
                _this->client = rfspace::connect(_this->hostname, _this->port, &_this->stream);
                _this->deviceInit();
            }
            catch (std::exception e) {
                spdlog::error("Could not connect to SDR: {0}", e.what());
            }
        }
        else if (connected && SmGui::Button("Disconnect##rfspace_source")) {
            _this->client->close();
        }
        if (_this->running) { SmGui::EndDisabled(); }


        if (connected) {
            if (_this->running) { SmGui::BeginDisabled(); }

            SmGui::LeftLabel("Samplerate");
            SmGui::FillWidth();
            if (SmGui::Combo("##rfspace_source_samp_rate", &_this->srId, _this->sampleRates.txt)) {
                _this->sampleRate = _this->sampleRates[_this->srId];
                _this->client->setSampleRate(_this->sampleRate);
                core::setInputSampleRate(_this->sampleRate);
                
                config.acquire();
                config.conf["devices"][_this->devConfName]["sampleRate"] = _this->sampleRates.key(_this->srId);
                config.release(true);
            }

            if (_this->running) { SmGui::EndDisabled(); }

            if (_this->client->deviceId == rfspace::RFSPACE_DEV_ID_CLOUD_IQ) {
                SmGui::LeftLabel("Antenna Port");
                SmGui::FillWidth();
                if (SmGui::Combo("##rfspace_source_rf_port", &_this->rfPortId, _this->rfPorts.txt)) {
                    _this->client->setPort(_this->rfPorts[_this->rfPortId]);

                    config.acquire();
                    config.conf["devices"][_this->devConfName]["rfPort"] = _this->rfPorts.key(_this->rfPortId);
                    config.release(true);
                }
            }

            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderFloatWithSteps("##rfspace_source_gain", &_this->gain, -30, 0, 10, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
                _this->client->setGain(_this->gain);

                config.acquire();
                config.conf["devices"][_this->devConfName]["gain"] = _this->gain;
                config.release(true);
            }

            SmGui::Text("Status:");
            SmGui::SameLine();
            SmGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), _this->connectedStr.c_str());
        }
        else {
            SmGui::Text("Status:");
            SmGui::SameLine();
            SmGui::Text("Not connected");
        }
    }

    void deviceInit() {
        // Generate the config name
        char buf[4096];
        sprintf(buf, "%s:%05d", hostname, port);
        devConfName = buf;
        sprintf(buf, "Connected (%s:%05d)", hostname, port);
        connectedStr = buf;

        // Get device name
        if (deviceNames.find(client->deviceId) != deviceNames.end()) {
            deviceName = deviceNames[client->deviceId];
        }
        else {
            deviceName = "Unknown";
        }
        
        // Create samplerate list
        auto srs = client->getValidSampleRates();
        sampleRates.clear();
        for (auto& sr : srs) {
            sampleRates.define(sr, getBandwdithScaled(sr), sr);
        }
        
        // Create RF port list
        rfPorts.clear();
        rfPorts.define("Port 1", rfspace::RFSPACE_RF_PORT_1);
        if (client->deviceId == rfspace::RFSPACE_DEV_ID_CLOUD_IQ) {
            rfPorts.define("Port 2", rfspace::RFSPACE_RF_PORT_2);
        }

        // Load config
        srId = 0;
        rfPortId = 0;
        bool changed = false;
        config.acquire();
        if (!config.conf["devices"].contains(devConfName)) {
            config.conf["devices"][devConfName]["sampleRate"] = sampleRates.key(0);
            config.conf["devices"][devConfName]["gain"] = 0;
            if (client->deviceId == rfspace::RFSPACE_DEV_ID_CLOUD_IQ) {
                config.conf["devices"][devConfName]["rfPort"] = rfPorts.key(0);
            }
            //changed = true;
        }
        if (config.conf["devices"][devConfName].contains("sampleRate")) {
            uint32_t sr = config.conf["devices"][devConfName]["sampleRate"];
            if (sampleRates.keyExists(sr)) {
                srId = sampleRates.keyId(sr);
            }
        }
        if (config.conf["devices"][devConfName].contains("gain")) {
            gain = config.conf["devices"][devConfName]["gain"];
        }
        if (config.conf["devices"][devConfName].contains("rfPort")) {
            std::string port = config.conf["devices"][devConfName]["rfPort"];
            if (rfPorts.keyExists(port)) {
                rfPortId = rfPorts.keyId(port);
            }
        }
        config.release(changed);

        // Set options
        sampleRate = sampleRates[srId];
        client->setSampleRate(sampleRate);
        core::setInputSampleRate(sampleRate);
        client->setFrequency(freq);
        client->setGain(gain);
        if (client->deviceId == rfspace::RFSPACE_DEV_ID_CLOUD_IQ) {
            client->setPort(rfPorts[rfPortId]);
        }

        spdlog::warn("End");
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    double sampleRate = 1228800;
    double freq;

    OptionList<uint32_t, uint32_t> sampleRates;
    int srId = 0;

    OptionList<std::string, rfspace::RFPort> rfPorts;
    int rfPortId = 0;

    float gain = 0;

    char hostname[1024];
    int port = 50000;
    std::string devConfName = "";
    std::string connectedStr = "";

    std::string deviceName = "Unknown";
    std::map<rfspace::DeviceID, std::string> deviceNames = {
        { rfspace::RFSPACE_DEV_ID_CLOUD_SDR, "CloudSDR" },
        { rfspace::RFSPACE_DEV_ID_CLOUD_IQ, "CloudIQ" },
        { rfspace::RFSPACE_DEV_ID_NET_SDR, "NetSDR" },
        { rfspace::RFSPACE_DEV_ID_SDR_IP, "SDR-IP" }
    };

    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;

    rfspace::RFspaceClient client;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["hostname"] = "192.168.0.111";
    def["port"] = 50000;
    def["devices"] = json::object();
    config.setPath(options::opts.root + "/rfspace_source_config.json");
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
    return new RFSpaceSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (RFSpaceSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}