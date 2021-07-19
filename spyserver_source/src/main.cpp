#include <spyserver_client.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <options.h>
#include <libairspyhf/airspyhf.h>
#include <gui/widgets/stepped_slider.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "spyserver_source",
    /* Description:     */ "Airspy HF+ source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

const char* deviceTypesStr[] = {
    "Unknown",
    "Airspy One",
    "Airspy HF+",
    "RTL-SDR"
};

const char* streamFormatStr = "UInt8\0"
                            "Int16\0"
                            "Float32\0";

const SpyServerStreamFormat streamFormats[] = {
    SPYSERVER_STREAM_FORMAT_UINT8,
    SPYSERVER_STREAM_FORMAT_INT16,
    SPYSERVER_STREAM_FORMAT_FLOAT
};

ConfigManager config;

class AirspyHFSourceModule : public ModuleManager::Instance {
public:
    AirspyHFSourceModule(std::string name) {
        this->name = name;

        config.acquire();
        std::string host = config.conf["hostname"];
        port = config.conf["port"];
        config.release();

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        strcpy(hostname, host.c_str());

        sigpath::sourceManager.registerSource("SpyServer", &handler);
    }

    ~AirspyHFSourceModule() {
        
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
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        gui::mainWindow.playButtonLocked = !(_this->client && _this->client->isOpen());
        spdlog::info("AirspyHFSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        gui::mainWindow.playButtonLocked = false;
        spdlog::info("AirspyHFSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (_this->running) {
            return;
        }
        
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FORMAT, streamFormats[_this->iqType]);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DECIMATION, _this->srId);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, _this->freq);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, 0);
        _this->client->setSetting(SPYSERVER_SETTING_STREAMING_MODE, SPYSERVER_STREAM_MODE_IQ_ONLY);
        
        _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
        _this->client->startStream();

        _this->running = true;
        spdlog::info("AirspyHFSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        
        _this->client->stopStream();

        _this->running = false;
        spdlog::info("AirspyHFSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (_this->running) {
            _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, freq);
        }
        _this->freq = freq;
        spdlog::info("AirspyHFSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        bool connected = (_this->client && _this->client->isOpen());
        gui::mainWindow.playButtonLocked = !connected;

        if (connected) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_rigctl_srv_host_", _this->name), _this->hostname, 1023)) {
            config.acquire();
            config.conf["hostname"] = _this->hostname;
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_rigctl_srv_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }
        if (connected) { style::endDisabled(); }

        if (_this->running) { style::beginDisabled(); }
        if (!connected && ImGui::Button("Connect##spyserver_source", ImVec2(menuWidth, 0))) {
            try {
                _this->client = spyserver::connect(_this->hostname, _this->port, &_this->stream);

                if (!_this->client->waitForDevInfo(3000)) {
                    spdlog::error("SpyServer didn't respond with device information");
                }
                else {
                    char buf[1024];
                    sprintf(buf, "%s [%08X]", deviceTypesStr[_this->client->devInfo.DeviceType], _this->client->devInfo.DeviceSerial);
                    _this->devRef = std::string(buf);

                    config.acquire();
                    if (!config.conf["devices"].contains(_this->devRef)) {
                        config.conf["devices"][_this->devRef]["sampleRateId"] = 0;
                        config.conf["devices"][_this->devRef]["sampleBitDepthId"] = 1;
                        config.conf["devices"][_this->devRef]["gainId"] = 0;
                    }
                    _this->srId = config.conf["devices"][_this->devRef]["sampleRateId"];
                    _this->iqType = config.conf["devices"][_this->devRef]["sampleBitDepthId"];
                    _this->gain = config.conf["devices"][_this->devRef]["gainId"];
                    config.release(true);

                    _this->gain = std::clamp<int>(_this->gain, 0, _this->client->devInfo.MaximumGainIndex);

                    // Refresh sample rates
                    _this->sampleRates.clear();
                    _this->sampleRatesTxt.clear();
                    for (int i = _this->client->devInfo.MinimumIQDecimation; i <= _this->client->devInfo.DecimationStageCount; i++) {
                        double sr = (double)_this->client->devInfo.MaximumSampleRate / ((double)(1 << i));
                        _this->sampleRates.push_back(sr);
                        _this->sampleRatesTxt += _this->getBandwdithScaled(sr);
                        _this->sampleRatesTxt += '\0';
                    }

                    _this->srId = std::clamp<int>(_this->srId, 0, _this->sampleRates.size()-1);

                    _this->sampleRate = _this->sampleRates[_this->srId];
                    core::setInputSampleRate(_this->sampleRate);
                    spdlog::info("Connected to server");
                }
            }
            catch (std::exception e) {
                spdlog::error("Could not connect to spyserver {0}", e.what());
            }
        }
        else if (connected && ImGui::Button("Disconnect##spyserver_source", ImVec2(menuWidth, 0))) {
            _this->client->close();
        }
        if (_this->running) { style::endDisabled(); }

        

        if (connected) {
            if (_this->running) { style::beginDisabled(); }
            ImGui::Text("Samplerate");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo("##spyserver_source_sr", &_this->srId, _this->sampleRatesTxt.c_str())) {
                _this->sampleRate = _this->sampleRates[_this->srId];
                core::setInputSampleRate(_this->sampleRate);
                config.acquire();
                config.conf["devices"][_this->devRef]["sampleRateId"] = _this->srId;
                config.release(true);
            }
            ImGui::Text("Sample bit depth");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo("##spyserver_source_type", &_this->iqType, streamFormatStr)) {
                config.acquire();
                config.conf["devices"][_this->devRef]["sampleBitDepthId"] = _this->iqType;
                config.release(true);
            }
            if (_this->running) { style::endDisabled(); }

            if (_this->client->devInfo.MaximumGainIndex) {
                ImGui::SetNextItemWidth(menuWidth);
                if (ImGui::SliderInt("##spyserver_source_gain", &_this->gain, 0, _this->client->devInfo.MaximumGainIndex)) {
                    _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
                    config.acquire();
                    config.conf["devices"][_this->devRef]["gainId"] = _this->gain;
                    config.release(true);
                }
            }            

            ImGui::Text("Status:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected (%s)", deviceTypesStr[_this->client->devInfo.DeviceType]);
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
    int port = 5555;
    int iqType = 0;

    int srId = 0;
    std::vector<double> sampleRates;
    std::string sampleRatesTxt;

    int gain = 0;

    std::string devRef = "";

    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;

    spyserver::SpyServerClient client;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["hostname"] = "localhost";
    def["port"] = 5555;
    def["devices"] = json::object();
    config.setPath(options::opts.root + "/spyserver_config.json");
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
    return new AirspyHFSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (AirspyHFSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}