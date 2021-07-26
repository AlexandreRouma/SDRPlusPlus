#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <iio.h>
#include <ad9361.h>
#include <options.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "plutosdr_source",
    /* Description:     */ "PlutoSDR source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

const char* gainModes[] = {
    "manual", "fast_attack", "slow_attack", "hybrid"
};

const char* gainModesTxt = "Manual\0Fast Attack\0Slow Attack\0Hybrid\0";

ConfigManager config;

class PlutoSDRSourceModule : public ModuleManager::Instance {
public:
    PlutoSDRSourceModule(std::string name) {
        this->name = name;

        config.acquire();
        std::string _ip = config.conf["IP"];
        strcpy(&ip[3], _ip.c_str());
        sampleRate = config.conf["sampleRate"];
        gainMode = config.conf["gainMode"];
        gain = config.conf["gain"];   
        config.release();     

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("PlutoSDR", &handler);
    }

    ~PlutoSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("PlutoSDR");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = true;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuSelected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("PlutoSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        spdlog::info("PlutoSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (_this->running) { return; }
        
        // TODO: INIT CONTEXT HERE
        _this->ctx = iio_create_context_from_uri(_this->ip);
        if (_this->ctx == NULL) {
            spdlog::error("Could not open pluto");
            return;
        }
	    _this->phy = iio_context_find_device(_this->ctx, "ad9361-phy");
        if (_this->phy == NULL) {
            spdlog::error("Could not connect to pluto phy");
            iio_context_destroy(_this->ctx);
            return;
        }
        _this->dev = iio_context_find_device(_this->ctx, "cf-ad9361-lpc");
        if (_this->dev == NULL) {
            spdlog::error("Could not connect to pluto dev");
            iio_context_destroy(_this->ctx);
            return;
        }

        // Configure pluto
        iio_channel_attr_write_bool(iio_device_find_channel(_this->phy, "altvoltage1", true), "powerdown", true);
        iio_channel_attr_write_bool(iio_device_find_channel(_this->phy, "altvoltage0", true), "powerdown", false);

        iio_channel_attr_write(iio_device_find_channel(_this->phy, "voltage0", false), "rf_port_select", "A_BALANCED");
        iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "altvoltage0", true), "frequency", round(_this->freq)); // Freq
	    iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false), "sampling_frequency", round(_this->sampleRate)); // Sample rate
        iio_channel_attr_write(iio_device_find_channel(_this->phy, "voltage0", false), "gain_control_mode", gainModes[_this->gainMode]); // manual gain
        iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false), "hardwaregain", round(_this->gain)); // gain
        ad9361_set_bb_rate(_this->phy, round(_this->sampleRate));
        
        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        spdlog::info("PlutoSDRSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();

        // DESTROY CONTEXT HERE
        if (_this->ctx != NULL) {
            iio_context_destroy(_this->ctx);
            _this->ctx = NULL;
        }

        spdlog::info("PlutoSDRSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            // SET PLUTO FREQ HERE
            iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "altvoltage0", true), "frequency", round(freq));
        }
        spdlog::info("PlutoSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::Text("IP");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputText(CONCAT("##_pluto_ip_", _this->name), &_this->ip[3], 16)) {
            config.acquire();
            config.conf["IP"] = &_this->ip[3];
            config.release(true);
        }
        
        ImGui::Text("Samplerate");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (_this->running) { style::beginDisabled(); }
        if (ImGui::InputFloat(CONCAT("##_samplerate_select_", _this->name), &_this->sampleRate, 1, 1000, 0)) {
            _this->sampleRate = std::clamp<float>(_this->sampleRate, 500000, 61000000);
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["sampleRate"] = _this->sampleRate;
            config.release(true);
        }
        if (_this->running) { style::endDisabled(); }

        ImGui::Text("Gain Mode");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_gainmode_select_", _this->name), &_this->gainMode, gainModesTxt)) {
            if (_this->running) {
                iio_channel_attr_write(iio_device_find_channel(_this->phy, "voltage0", false), "gain_control_mode", gainModes[_this->gainMode]);
            }
            config.acquire();
            config.conf["gainMode"] = _this->gainMode;
            config.release(true);
        }

        ImGui::Text("PGA Gain");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (_this->gainMode) { style::beginDisabled(); }
        if (ImGui::SliderFloat(CONCAT("##_gain_select_", _this->name), &_this->gain, 0, 76)) {
            if (_this->running) {
                iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false),"hardwaregain", round(_this->gain));
            }
            config.acquire();
            config.conf["gain"] = _this->gain;
            config.release(true);
        }
        if (_this->gainMode) { style::endDisabled(); }
    }

    static void worker(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        int blockSize = _this->sampleRate / 200.0f;

        struct iio_channel *rx0_i, *rx0_q;
        struct iio_buffer *rxbuf;
    
        rx0_i = iio_device_find_channel(_this->dev, "voltage0", 0);
        rx0_q = iio_device_find_channel(_this->dev, "voltage1", 0);
    
        iio_channel_enable(rx0_i);
        iio_channel_enable(rx0_q);
    
        rxbuf = iio_device_create_buffer(_this->dev, blockSize, false);
        if (!rxbuf) {
            spdlog::error("Could not create RX buffer");
            return;
        }

        while (true) {
            // Read samples here
            // TODO: RECEIVE HERE
            iio_buffer_refill(rxbuf);

            int16_t* buf = (int16_t*)iio_buffer_first(rxbuf, rx0_i);

            for (int i = 0; i < blockSize; i++) {
                _this->stream.writeBuf[i].re = (float)buf[i * 2] / 32768.0f;
                _this->stream.writeBuf[i].im = (float)buf[(i * 2) + 1] / 32768.0f;
            }

            volk_16i_s32f_convert_32f((float*)_this->stream.writeBuf, buf, 32768.0f, blockSize*2);

            if (!_this->stream.swap(blockSize)) { break; };
        }

        iio_buffer_destroy(rxbuf);
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    float sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    struct iio_context *ctx = NULL;
	struct iio_device *phy = NULL;
    struct iio_device *dev = NULL;
    bool running = false;
    bool ipMode = true;
    double freq;
    char ip[1024] = "ip:192.168.2.1";
    int gainMode = 0;
    float gain = 0;
};

MOD_EXPORT void _INIT_() {
    json defConf;
    defConf["IP"] = "192.168.2.1";
    defConf["sampleRate"] = 4000000.0f;
    defConf["gainMode"] = 0;
    defConf["gain"] = 0.0f;
    config.setPath(options::opts.root + "/plutosdr_source_config.json");
    config.load(defConf);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new PlutoSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (PlutoSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}