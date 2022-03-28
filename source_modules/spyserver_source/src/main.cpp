#include <spyserver_client.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/widgets/stepped_slider.h>
#include <gui/smgui.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "spyserver_source",
    /* Description:     */ "SpyServer source module for SDR++",
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

const int streamFormatsBitCount[] = {
    8,
    16,
    32
};

ConfigManager config;

class SpyServerSourceModule : public ModuleManager::Instance {
public:
    SpyServerSourceModule(std::string name) {
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

    ~SpyServerSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("SpyServer");
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

        int srvBits = streamFormatsBitCount[_this->iqType];
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FORMAT, streamFormats[_this->iqType]);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DECIMATION, _this->srId + _this->client->devInfo.MinimumIQDecimation);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, _this->freq);
        _this->client->setSetting(SPYSERVER_SETTING_STREAMING_MODE, SPYSERVER_STREAM_MODE_IQ_ONLY);
        _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->srId + _this->client->devInfo.MinimumIQDecimation));
        _this->client->startStream();

        _this->running = true;
        spdlog::info("SpyServerSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (!_this->running) { return; }

        _this->client->stopStream();

        _this->running = false;
        spdlog::info("SpyServerSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (_this->running) {
            _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, freq);
        }
        _this->freq = freq;
        spdlog::info("SpyServerSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;

        bool connected = (_this->client && _this->client->isOpen());
        gui::mainWindow.playButtonLocked = !connected;

        if (connected) { SmGui::BeginDisabled(); }
        if (SmGui::InputText(CONCAT("##_spyserver_srv_host_", _this->name), _this->hostname, 1023)) {
            config.acquire();
            config.conf["hostname"] = _this->hostname;
            config.release(true);
        }
        SmGui::SameLine();
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_spyserver_srv_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }
        if (connected) { SmGui::EndDisabled(); }

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (!connected && SmGui::Button("Connect##spyserver_source")) {
            try {
                if (_this->client) { _this->client.reset(); }
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

                    _this->srId = std::clamp<int>(_this->srId, 0, _this->sampleRates.size() - 1);

                    _this->sampleRate = _this->sampleRates[_this->srId];
                    core::setInputSampleRate(_this->sampleRate);
                    spdlog::info("Connected to server");
                }
            }
            catch (std::exception e) {
                spdlog::error("Could not connect to spyserver {0}", e.what());
            }
        }
        else if (connected && SmGui::Button("Disconnect##spyserver_source")) {
            _this->client->close();
        }
        if (_this->running) { SmGui::EndDisabled(); }


        if (connected) {
            if (_this->running) { style::beginDisabled(); }
            SmGui::LeftLabel("Samplerate");
            SmGui::FillWidth();
            if (SmGui::Combo("##spyserver_source_sr", &_this->srId, _this->sampleRatesTxt.c_str())) {
                _this->sampleRate = _this->sampleRates[_this->srId];
                core::setInputSampleRate(_this->sampleRate);
                config.acquire();
                config.conf["devices"][_this->devRef]["sampleRateId"] = _this->srId;
                config.release(true);
            }
            if (_this->running) { style::endDisabled(); }

            SmGui::LeftLabel("Sample bit depth");
            SmGui::FillWidth();
            if (SmGui::Combo("##spyserver_source_type", &_this->iqType, streamFormatStr)) {
                int srvBits = streamFormatsBitCount[_this->iqType];
                _this->client->setSetting(SPYSERVER_SETTING_IQ_FORMAT, streamFormats[_this->iqType]);
                _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->srId + _this->client->devInfo.MinimumIQDecimation));

                config.acquire();
                config.conf["devices"][_this->devRef]["sampleBitDepthId"] = _this->iqType;
                config.release(true);
            }

            if (_this->client->devInfo.MaximumGainIndex) {
                SmGui::FillWidth();
                if (SmGui::SliderInt("##spyserver_source_gain", (int*)&_this->gain, 0, _this->client->devInfo.MaximumGainIndex)) {
                    int srvBits = streamFormatsBitCount[_this->iqType];
                    _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
                    _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->srId + _this->client->devInfo.MinimumIQDecimation));
                    config.acquire();
                    config.conf["devices"][_this->devRef]["gainId"] = _this->gain;
                    config.release(true);
                }
            }

            SmGui::Text("Status:");
            SmGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected (%s)", deviceTypesStr[_this->client->devInfo.DeviceType]);
        }
        else {
            SmGui::Text("Status:");
            SmGui::SameLine();
            SmGui::Text("Not connected");
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

    uint32_t gain = 0;

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
    config.setPath(core::args["root"].s() + "/spyserver_config.json");
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