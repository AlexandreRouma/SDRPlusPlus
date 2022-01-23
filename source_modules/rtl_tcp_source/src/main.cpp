#include <rtltcp_client.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <options.h>
#include <gui/smgui.h>
#include <gui/style.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "rtl_tcp_source",
    /* Description:     */ "RTL-TCP source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const double sampleRates[] = {
    250000,
    1024000,
    1536000,
    1792000,
    1920000,
    2048000,
    2160000,
    2400000,
    2560000,
    2880000,
    3200000
};

const char* sampleRatesTxt[] = {
    "250KHz",
    "1.024MHz",
    "1.536MHz",
    "1.792MHz",
    "1.92MHz",
    "2.048MHz",
    "2.16MHz",
    "2.4MHz",
    "2.56MHz",
    "2.88MHz",
    "3.2MHz"
};

#define SAMPLE_RATE_COUNT (sizeof(sampleRates) / sizeof(double))

class RTLTCPSourceModule : public ModuleManager::Instance {
public:
    RTLTCPSourceModule(std::string name) {
        this->name = name;

        sampleRate = 2400000.0;

        int _24id = 0;
        for (int i = 0; i < SAMPLE_RATE_COUNT; i++) {
            if (sampleRates[i] == 2400000) { _24id = i; }
            srTxt += sampleRatesTxt[i];
            srTxt += '\0';
        }
        srId = 7;

        config.acquire();
        std::string hostStr = config.conf["host"];
        port = config.conf["port"];
        double wantedSr = config.conf["sampleRate"];
        directSamplingMode = config.conf["directSamplingMode"];
        ppm = config.conf["ppm"];
        rtlAGC = config.conf["rtlAGC"];
        tunerAGC = config.conf["tunerAGC"];
        gain = std::clamp<int>(config.conf["gainIndex"], 0, 28);
        biasTee = config.conf["biasTee"];
        offsetTuning = config.conf["offsetTuning"];
        hostStr = hostStr.substr(0, 1023);
        strcpy(ip, hostStr.c_str());
        config.release();

        bool found = false;
        for (int i = 0; i < SAMPLE_RATE_COUNT; i++) {
            if (sampleRates[i] == wantedSr) {
                found = true;
                srId = i;
                sampleRate = sampleRates[i];
                break;
            }
        }
        if (!found) {
            srId = _24id;
            sampleRate = sampleRates[_24id];
        }

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("RTL-TCP", &handler);
    }

    ~RTLTCPSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("RTL-TCP");
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
    static void menuSelected(void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("RTLTCPSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;
        spdlog::info("RTLTCPSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;
        if (_this->running) { return; }
        if (!_this->client.connectToRTL(_this->ip, _this->port)) {
            spdlog::error("Could not connect to {0}:{1}", _this->ip, _this->port);
            return;
        }
        spdlog::warn("Setting sample rate to {0}", _this->sampleRate);
        _this->client.setFrequency(_this->freq);
        _this->client.setSampleRate(_this->sampleRate);
        _this->client.setPPM(_this->ppm);
        _this->client.setDirectSampling(_this->directSamplingMode);
        _this->client.setAGCMode(_this->rtlAGC);
        _this->client.setBiasTee(_this->biasTee);
        _this->client.setOffsetTuning(_this->offsetTuning);
        if (_this->tunerAGC) {
            _this->client.setGainMode(0);
        }
        else {
            _this->client.setGainMode(1);

            // Setting it twice because for some reason it refuses to do it on the first time
            _this->client.setGainIndex(_this->gain);
        }

        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        spdlog::info("RTLTCPSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        _this->client.disconnect();
        spdlog::info("RTLTCPSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;
        if (_this->running) {
            _this->client.setFrequency(freq);
        }
        _this->freq = freq;
        spdlog::info("RTLTCPSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        if (SmGui::InputText(CONCAT("##_ip_select_", _this->name), _this->ip, 1024)) {
            config.acquire();
            config.conf["host"] = std::string(_this->ip);
            config.release(true);
        }
        SmGui::SameLine();
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_port_select_", _this->name), &_this->port, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }

        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_rtltcp_sr_", _this->name), &_this->srId, _this->srTxt.c_str())) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["sampleRate"] = _this->sampleRate;
            config.release(true);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Direct Sampling");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_rtltcp_ds_", _this->name), &_this->directSamplingMode, "Disabled\0I branch\0Q branch\0")) {
            if (_this->running) {
                _this->client.setDirectSampling(_this->directSamplingMode);
                _this->client.setGainIndex(_this->gain);
            }
            config.acquire();
            config.conf["directSamplingMode"] = _this->directSamplingMode;
            config.release(true);
        }

        SmGui::LeftLabel("PPM Correction");
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_rtltcp_ppm_", _this->name), &_this->ppm, 1, 10)) {
            if (_this->running) {
                _this->client.setPPM(_this->ppm);
            }
            config.acquire();
            config.conf["ppm"] = _this->ppm;
            config.release(true);
        }

        if (_this->tunerAGC) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_gain_select_", _this->name), &_this->gain, 0, 28, SmGui::FMT_STR_NONE)) {
            if (_this->running) {
                _this->client.setGainIndex(_this->gain);
            }
            config.acquire();
            config.conf["gainIndex"] = _this->gain;
            config.release(true);
        }
        if (_this->tunerAGC) { SmGui::EndDisabled(); }

        if (SmGui::Checkbox(CONCAT("Bias-T##_biast_select_", _this->name), &_this->biasTee)) {
            if (_this->running) {
                _this->client.setBiasTee(_this->biasTee);
            }
            config.acquire();
            config.conf["biasTee"] = _this->biasTee;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Offset Tuning##_biast_select_", _this->name), &_this->offsetTuning)) {
            if (_this->running) {
                _this->client.setOffsetTuning(_this->offsetTuning);
            }
            config.acquire();
            config.conf["offsetTuning"] = _this->offsetTuning;
            config.release(true);
        }

        if (SmGui::Checkbox("RTL AGC", &_this->rtlAGC)) {
            if (_this->running) {
                _this->client.setAGCMode(_this->rtlAGC);
                if (!_this->rtlAGC) {
                    _this->client.setGainIndex(_this->gain);
                }
            }
            config.acquire();
            config.conf["rtlAGC"] = _this->rtlAGC;
            config.release(true);
        }

        SmGui::ForceSync();
        if (SmGui::Checkbox("Tuner AGC", &_this->tunerAGC)) {
            if (_this->running) {
                _this->client.setGainMode(!_this->tunerAGC);
                if (!_this->tunerAGC) {
                    _this->client.setGainIndex(_this->gain);
                }
            }
            config.acquire();
            config.conf["tunerAGC"] = _this->tunerAGC;
            config.release(true);
        }
    }

    static void worker(void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;
        int blockSize = _this->sampleRate / 200.0f;
        uint8_t* inBuf = new uint8_t[blockSize * 2];

        while (true) {
            // Read samples here
            _this->client.receiveData(inBuf, blockSize * 2);
            for (int i = 0; i < blockSize; i++) {
                _this->stream.writeBuf[i].re = ((double)inBuf[i * 2] - 128.0) / 128.0;
                _this->stream.writeBuf[i].im = ((double)inBuf[(i * 2) + 1] - 128.0) / 128.0;
            }
            if (!_this->stream.swap(blockSize)) { break; };
        }

        delete[] inBuf;
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    RTLTCPClient client;
    bool running = false;
    double freq;
    char ip[1024] = "localhost";
    int port = 1234;
    int gain = 0;
    int ppm = 0;
    bool rtlAGC = false;
    bool tunerAGC = false;
    int directSamplingMode = 0;
    int srId = 0;
    bool biasTee = false;
    bool offsetTuning = false;

    std::string srTxt = "";
};

MOD_EXPORT void _INIT_() {
    config.setPath(options::opts.root + "/rtl_tcp_config.json");
    json defConf;
    defConf["host"] = "localhost";
    defConf["port"] = 1234;
    defConf["sampleRate"] = 2400000.0;
    defConf["directSamplingMode"] = 0;
    defConf["ppm"] = 0;
    defConf["rtlAGC"] = false;
    defConf["tunerAGC"] = false;
    defConf["gainIndex"] = 0;
    defConf["biasTee"] = false;
    defConf["offsetTuning"] = false;
    config.load(defConf);
    config.enableAutoSave();

    config.acquire();
    if (!config.conf.contains("biasTee")) {
        config.conf["biasTee"] = false;
    }
    if (!config.conf.contains("offsetTuning")) {
        config.conf["offsetTuning"] = false;
    }
    if (!config.conf.contains("ppm")) {
        config.conf["ppm"] = 0;
    }
    if (!config.conf.contains("sampleRate")) {
        config.conf["sampleRate"] = 2400000.0;
    }
    config.release(true);
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RTLTCPSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (RTLTCPSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}