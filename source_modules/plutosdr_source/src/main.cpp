#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <gui/smgui.h>
#include <iio.h>
#include <ad9361.h>
#include <utils/optionlist.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
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

        // Load configuration
        config.acquire();
        std::string _ip = config.conf["IP"];
        strcpy(&ip[3], _ip.c_str());
        sampleRate = config.conf["sampleRate"];
        gainMode = config.conf["gainMode"];
        gain = config.conf["gain"];
        config.release();

        // Define valid samplerates
        for (double sr = 1000000.0; sr <= 61440000.0; sr += 500000.0) {
            samplerates.define(sr, getBandwdithScaled(sr), sr);
        }
        samplerates.define(61440000, getBandwdithScaled(61440000.0), 61440000.0);

        // Set samplerate ID
        if (samplerates.keyExists(sampleRate)) {
            srId = samplerates.keyId(sampleRate);
        }
        else {
            srId = 0;
            sampleRate = samplerates.value(srId);
        }

        // Register source
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
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("PlutoSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        flog::info("PlutoSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (_this->running) { return; }

        // Open device
        _this->ctx = iio_create_context_from_uri(_this->ip);
        if (_this->ctx == NULL) {
            flog::error("Could not open pluto");
            return;
        }
        _this->phy = iio_context_find_device(_this->ctx, "ad9361-phy");
        if (_this->phy == NULL) {
            flog::error("Could not connect to pluto phy");
            iio_context_destroy(_this->ctx);
            return;
        }
        _this->dev = iio_context_find_device(_this->ctx, "cf-ad9361-lpc");
        if (_this->dev == NULL) {
            flog::error("Could not connect to pluto dev");
            iio_context_destroy(_this->ctx);
            return;
        }

        // Enable RX channel and disable TX
        iio_channel_attr_write_bool(iio_device_find_channel(_this->phy, "altvoltage1", true), "powerdown", true);
        iio_channel_attr_write_bool(iio_device_find_channel(_this->phy, "altvoltage0", true), "powerdown", false);

        // Configure RX channel
        iio_channel_attr_write(iio_device_find_channel(_this->phy, "voltage0", false), "rf_port_select", "A_BALANCED");
        iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "altvoltage0", true), "frequency", round(_this->freq));              // Freq
        iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false), "sampling_frequency", round(_this->sampleRate)); // Sample rate
        iio_channel_attr_write(iio_device_find_channel(_this->phy, "voltage0", false), "gain_control_mode", gainModes[_this->gainMode]);         // manual gain
        iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false), "hardwaregain", round(_this->gain));             // gain
        ad9361_set_bb_rate(_this->phy, round(_this->sampleRate));

        // Start worker thread
        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        flog::info("PlutoSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (!_this->running) { return; }

        // Stop worker thread
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();

        // Close device
        if (_this->ctx != NULL) {
            iio_context_destroy(_this->ctx);
            _this->ctx = NULL;
        }

        flog::info("PlutoSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            // Tune device
            iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "altvoltage0", true), "frequency", round(freq));
        }
        flog::info("PlutoSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::LeftLabel("IP");
        SmGui::FillWidth();
        if (SmGui::InputText(CONCAT("##_pluto_ip_", _this->name), &_this->ip[3], 16)) {
            config.acquire();
            config.conf["IP"] = &_this->ip[3];
            config.release(true);
        }

        SmGui::LeftLabel("Samplerate");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_pluto_sr_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["sampleRate"] = _this->sampleRate;
            config.release(true);
        }
        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Gain Mode");
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_gainmode_select_", _this->name), &_this->gainMode, gainModesTxt)) {
            if (_this->running) {
                iio_channel_attr_write(iio_device_find_channel(_this->phy, "voltage0", false), "gain_control_mode", gainModes[_this->gainMode]);
            }
            config.acquire();
            config.conf["gainMode"] = _this->gainMode;
            config.release(true);
        }

        SmGui::LeftLabel("PGA Gain");
        if (_this->gainMode) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        if (SmGui::SliderFloat(CONCAT("##_gain_select_", _this->name), &_this->gain, 0, 76)) {
            if (_this->running) {
                iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false), "hardwaregain", round(_this->gain));
            }
            config.acquire();
            config.conf["gain"] = _this->gain;
            config.release(true);
        }
        if (_this->gainMode) { SmGui::EndDisabled(); }
    }

    static void worker(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        int blockSize = _this->sampleRate / 200.0f;

        // Acquire channels
        iio_channel* rx0_i = iio_device_find_channel(_this->dev, "voltage0", 0);
        iio_channel* rx0_q = iio_device_find_channel(_this->dev, "voltage1", 0);

        // Start streaming
        iio_channel_enable(rx0_i);
        iio_channel_enable(rx0_q);

        // Allocate buffer
        iio_buffer* rxbuf = iio_device_create_buffer(_this->dev, blockSize, false);
        if (!rxbuf) {
            flog::error("Could not create RX buffer");
            return;
        }

        while (true) {
            // Read samples
            iio_buffer_refill(rxbuf);

            // Get buffer pointer
            int16_t* buf = (int16_t*)iio_buffer_first(rxbuf, rx0_i);

            // Convert samples to CF32
            volk_16i_s32f_convert_32f((float*)_this->stream.writeBuf, buf, 32768.0f, blockSize * 2);

            // Send out the samples
            if (!_this->stream.swap(blockSize)) { break; };
        }

        // Stop streaming
        iio_channel_disable(rx0_i);
        iio_channel_disable(rx0_q);

        // Free buffer
        iio_buffer_destroy(rxbuf);
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    float sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    iio_context* ctx = NULL;
    iio_device* phy = NULL;
    iio_device* dev = NULL;
    bool running = false;

    bool ipMode = true;
    double freq;
    char ip[1024] = "ip:192.168.2.1";
    int gainMode = 0;
    float gain = 0;
    int srId = 0;

    OptionList<int, double> samplerates;
};

MOD_EXPORT void _INIT_() {
    json defConf;
    defConf["IP"] = "192.168.2.1";
    defConf["sampleRate"] = 4000000.0f;
    defConf["gainMode"] = 0;
    defConf["gain"] = 0.0f;
    config.setPath(core::args["root"].s() + "/plutosdr_source_config.json");
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