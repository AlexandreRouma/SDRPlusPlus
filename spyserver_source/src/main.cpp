#include <spyserver_client.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <new_module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "spyserver_source",
    /* Description:     */ "SpyServer source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

class SpyServerSourceModule : public ModuleManager::Instance {
public:
    SpyServerSourceModule(std::string name) {
        this->name = name;

        sampleRate = 2560000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &client.iqStream;
        sigpath::sourceManager.registerSource("SpyServer", &handler);
    }

    ~SpyServerSourceModule() {
        
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
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("SpyServerSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        spdlog::info("SpyServerSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (_this->running) {
            return;
        }
        if (!_this->client.connectToSpyserver(_this->ip, _this->port)) {
            spdlog::error("Could not connect to {0}:{1}", _this->ip, _this->port);
            return;
        }
        _this->client.tune(_this->freq);
        _this->client.setSampleRate(_this->sampleRate);
        //_this->client.setGainIndex(_this->gain);
        //_this->client.setGainMode(!_this->tunerAGC);
        //_this->client.setDirectSampling(_this->directSamplingMode);
        //_this->client.setAGCMode(_this->rtlAGC);
        _this->running = true;
        spdlog::info("SpyServerSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->client.disconnect();
        spdlog::info("SpyServerSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (_this->running) {
            _this->client.tune(freq);
        }
        _this->freq = freq;
        spdlog::info("SpyServerSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();
        float portWidth = ImGui::CalcTextSize("00000").x + 20;

        ImGui::SetNextItemWidth(menuWidth - portWidth);
        ImGui::InputText(CONCAT("##_ip_select_", _this->name), _this->ip, 1024);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(portWidth);
        ImGui::InputInt(CONCAT("##_port_select_", _this->name), &_this->port, 0);

        ImGui::SetNextItemWidth(ImGui::CalcTextSize("OOOOOOOOOO").x);
        if (ImGui::Combo("Direct sampling", &_this->directSamplingMode, "Disabled\0I branch\0Q branch\0")) {
            if (_this->running) {
                //_this->client.setDirectSampling(_this->directSamplingMode);
            }
        }

        if (ImGui::Checkbox("RTL AGC", &_this->rtlAGC)) {
            if (_this->running) {
                //_this->client.setAGCMode(_this->rtlAGC);
            }
        }

        if (ImGui::Checkbox("Tuner AGC", &_this->tunerAGC)) {
            if (_this->running) {
                //_this->client.setGainMode(!_this->tunerAGC);
                if (!_this->tunerAGC) {
                    //_this->client.setGainIndex(_this->gain);
                }
            }
        }

        if (_this->tunerAGC) { style::beginDisabled(); }
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::SliderInt(CONCAT("##_gain_select_", _this->name), &_this->gain, 0, 29, "")) {
            if (_this->running) {
                //_this->client.setGainIndex(_this->gain);
            }
        }
        if (_this->tunerAGC) { style::endDisabled(); }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    SpyServerClient client;
    bool running = false;
    double freq;
    char ip[1024] = "localhost";
    int port = 5555;
    int gain = 0;
    bool rtlAGC = false;
    bool tunerAGC = false;
    int directSamplingMode = 0;
};

MOD_EXPORT void _INIT_() {
   // Do your one time init here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SpyServerSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (SpyServerSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // Do your one shutdown here
}