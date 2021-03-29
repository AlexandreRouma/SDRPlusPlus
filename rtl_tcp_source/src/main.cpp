#include <rtltcp_client.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <options.h>
#include <gui/style.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
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
    "2.56MHz",
    "2.88MHz",
    "3.2MHz"
};

class RTLTCPSourceModule : public ModuleManager::Instance {
public:
    RTLTCPSourceModule(std::string name) {
        this->name = name;

        sampleRate = 2560000.0;

        int srCount = sizeof(sampleRatesTxt) / sizeof(char*);
        for (int i = 0; i < srCount; i++) {
            srTxt += sampleRatesTxt[i];
            srTxt += '\0';
        }
        srId = 7;

        config.aquire();
        std::string hostStr = config.conf["host"];
        port = config.conf["port"];
        directSamplingMode = config.conf["directSamplingMode"];
        rtlAGC = config.conf["rtlAGC"];
        tunerAGC = config.conf["tunerAGC"];
        gain = config.conf["gainIndex"];
        hostStr = hostStr.substr(0, 1023);
        strcpy(ip, hostStr.c_str());
        config.release();

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
        if (_this->running) {
            return;
        }
        if (!_this->client.connectToRTL(_this->ip, _this->port)) {
            spdlog::error("Could not connect to {0}:{1}", _this->ip, _this->port);
            return;
        }
        spdlog::warn("Setting sample rate to {0}", _this->sampleRate);
        _this->client.setFrequency(_this->freq);
        _this->client.setSampleRate(_this->sampleRate);
        _this->client.setGainMode(!_this->tunerAGC);
        _this->client.setDirectSampling(_this->directSamplingMode);
        _this->client.setAGCMode(_this->rtlAGC);
        _this->client.setGainIndex(_this->gain);
        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        spdlog::info("RTLTCPSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        RTLTCPSourceModule* _this = (RTLTCPSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
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
        float menuWidth = ImGui::GetContentRegionAvailWidth();
        float portWidth = ImGui::CalcTextSize("00000").x + 20;

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth - portWidth);
        ImGui::InputText(CONCAT("##_ip_select_", _this->name), _this->ip, 1024);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(portWidth);
        ImGui::InputInt(CONCAT("##_port_select_", _this->name), &_this->port, 0);

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_rtltcp_sr_", _this->name), &_this->srId, _this->srTxt.c_str())) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { style::endDisabled(); }

        ImGui::SetNextItemWidth(ImGui::CalcTextSize("OOOOOOOOOO").x);
        if (ImGui::Combo(CONCAT("Direct Sampling##_rtltcp_ds_", _this->name), &_this->directSamplingMode, "Disabled\0I branch\0Q branch\0")) {
            if (_this->running) {
                _this->client.setDirectSampling(_this->directSamplingMode);
                _this->client.setGainIndex(_this->gain);
            }
        }

        if (ImGui::Checkbox("RTL AGC", &_this->rtlAGC)) {
            if (_this->running) {
                _this->client.setAGCMode(_this->rtlAGC);
                if (!_this->rtlAGC) {
                    _this->client.setGainIndex(_this->gain);
                }
            }
        }

        if (ImGui::Checkbox("Tuner AGC", &_this->tunerAGC)) {
            if (_this->running) {
                _this->client.setGainMode(!_this->tunerAGC);
                if (!_this->tunerAGC) {
                    _this->client.setGainIndex(_this->gain);
                }
            }
        }

        if (_this->tunerAGC) { style::beginDisabled(); }
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::SliderInt(CONCAT("##_gain_select_", _this->name), &_this->gain, 0, 29, "")) {
            if (_this->running) {
                _this->client.setGainIndex(_this->gain);
            }
        }
        if (_this->tunerAGC) { style::endDisabled(); }
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
    bool rtlAGC = false;
    bool tunerAGC = false;
    int directSamplingMode = 0;
    int srId = 0;

    std::string srTxt = "";
};

MOD_EXPORT void _INIT_() {
   config.setPath(options::opts.root + "/rtl_tcp_config.json");
   json defConf;
   defConf["host"] = "localhost";
   defConf["port"] = 1234;
   defConf["directSamplingMode"] = 0;
   defConf["rtlAGC"] = false;
   defConf["tunerAGC"] = false;
   defConf["gainIndex"] = 0;
   config.load(defConf);
   config.enableAutoSave();
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