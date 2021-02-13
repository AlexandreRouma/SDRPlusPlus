#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <options.h>
#include <rtl-sdr.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "rtl_sdr_source",
    /* Description:     */ "RTL-SDR source module for SDR++",
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

const char* directSamplingModesTxt = "Disabled\0I branch\0Q branch\0";

class RTLSDRSourceModule : public ModuleManager::Instance {
public:
    RTLSDRSourceModule(std::string name) {
        this->name = name;

        sampleRate = sampleRates[0];

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected; 
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        for (int i = 0; i < 10; i++) {
            sampleRateListTxt += sampleRatesTxt[i];
            sampleRateListTxt += '\0';
        }

        refresh();

        // Select device from config here
        selectFirst();

        core::setInputSampleRate(sampleRate);

        sigpath::sourceManager.registerSource("RTL-SDR", &handler);
    }

    ~RTLSDRSourceModule() {
        
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

    void refresh() {
        devNames.clear();
        devListTxt = "";

        devCount = rtlsdr_get_device_count();
        char buf[1024];
        for (int i = 0; i < devCount; i++) {
            const char* devName = rtlsdr_get_device_name(i);
            sprintf(buf, "%s [%d]", devName, i);
            devNames.push_back(buf);
            devListTxt += buf;
            devListTxt += '\0';
        }
    }

    void selectFirst() {
        if (devCount > 0) {
            selectById(0);
        }
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devCount; i++) {
            if (name == devNames[i]) {
                selectById(i);
                return;
            }
        }
        selectFirst();
    }

    void selectById(int id) {
        selectedDevName = devNames[id];

        if (rtlsdr_open(&openDev, id) < 0) {
            spdlog::error("Could not open RTL-SDR");
            return;
        }

        gainList.clear();
        int gains[256];
        int n = rtlsdr_get_tuner_gains(openDev, gains);
        gainList = std::vector<int>(gains, gains + n);
        std::sort(gainList.begin(), gainList.end());
        if (gainId >= gainList.size()) { gainId = gainList.size() - 1; }

        // Create config if needed

        // Load config

        rtlsdr_close(openDev);
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
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("RTLSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        spdlog::info("RTLSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (_this->running) {
            return;
        }

        if (rtlsdr_open(&_this->openDev, _this->devId) < 0) {
            spdlog::error("Could not open RTL-SDR");
            return;
        }
        
        rtlsdr_set_sample_rate(_this->openDev, _this->sampleRate);
        rtlsdr_set_center_freq(_this->openDev, _this->freq);
        rtlsdr_set_tuner_bandwidth(_this->openDev, 0);
        rtlsdr_set_direct_sampling(_this->openDev, _this->directSamplingMode);
        rtlsdr_set_agc_mode(_this->openDev, _this->directSamplingMode);
        rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
        if (_this->tunerAgc) {
            rtlsdr_set_tuner_gain_mode(_this->openDev, 0);
        }
        else {
            rtlsdr_set_tuner_gain_mode(_this->openDev, 1);
            rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
        }

        _this->asyncCount = (int)roundf(_this->sampleRate / (200 * 512)) * 512;

        _this->workerThread = std::thread(&RTLSDRSourceModule::worker, _this);

        _this->running = true;
        spdlog::info("RTLSDRSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->stream.stopWriter();
        rtlsdr_cancel_async(_this->openDev);
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();
        rtlsdr_close(_this->openDev);
        spdlog::info("RTLSDRSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        if (_this->running) {
            rtlsdr_set_center_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        spdlog::info("RTLSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_airspy_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            // Select RTL device here
            core::setInputSampleRate(_this->sampleRate);
            // Save config here
        }

        if (ImGui::Combo(CONCAT("##_airspy_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            // Save config here
        }

        ImGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (ImGui::Button(CONCAT("Refresh##_airspy_refr_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
            
            // Reselect dev from config here
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { style::endDisabled(); }

        // Rest of rtlsdr config here
        ImGui::Text("Direct Sampling");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_rtlsdr_ds_", _this->name), &_this->directSamplingMode, directSamplingModesTxt)) {
            if (_this->running) {
                rtlsdr_set_direct_sampling(_this->openDev, _this->directSamplingMode);
            }
            // Save config here
        }

        if (ImGui::Checkbox(CONCAT("RTL AGC##_rtlsdr_rtl_agc_", _this->name), &_this->rtlAgc)) {
            if (_this->running) {
                rtlsdr_set_agc_mode(_this->openDev, _this->rtlAgc);
            }
            // Save config here
        }

        if (ImGui::Checkbox(CONCAT("Tuner AGC##_rtlsdr_tuner_agc_", _this->name), &_this->tunerAgc)) {
            if (_this->running) {
                if (_this->tunerAgc) {
                    rtlsdr_set_tuner_gain_mode(_this->openDev, 0);
                }
                else {
                    rtlsdr_set_tuner_gain_mode(_this->openDev, 1);
                    rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
                }
            }
            // Save config here
        }

        if (_this->tunerAgc) { style::beginDisabled(); }
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::SliderInt(CONCAT("##_rtlsdr_gain_", _this->name), &_this->gainId, 0, _this->gainList.size() - 1, "")) {
            if (_this->running) {
                rtlsdr_set_tuner_gain(_this->openDev, _this->gainList[_this->gainId]);
            }
            // Save config here
        }
        if (_this->tunerAgc) { style::endDisabled(); }
    }

    void worker() {
        rtlsdr_reset_buffer(openDev);
        rtlsdr_read_async(openDev, asyncHandler, this, 0, asyncCount);
    }

    static void asyncHandler(unsigned char *buf, uint32_t len, void *ctx) {
        RTLSDRSourceModule* _this = (RTLSDRSourceModule*)ctx;

        int sampCount = len / 2;
        for (int i = 0; i < sampCount; i++) {
            _this->stream.writeBuf[i].i = (float)(buf[(i * 2) + 1] - 127) / 128.0f;
            _this->stream.writeBuf[i].q = (float)(buf[i * 2] - 127) / 128.0f;
        }
        if (!_this->stream.swap(sampCount)) { return; }
    }

    std::string name;
    rtlsdr_dev_t* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    std::string selectedDevName = "";
    int devId = 0;
    int srId = 0;
    int devCount = 0;
    std::thread workerThread;

    bool biasT = false;

    int gainId = 0;
    std::vector<int> gainList;

    bool rtlAgc = false;
    bool tunerAgc = false;

    int directSamplingMode = 0;

    // Handler stuff
    int asyncCount = 0;

    std::vector<std::string> devNames;
    std::string devListTxt;
    std::string sampleRateListTxt;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(options::opts.root + "/rtl_sdr_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RTLSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (RTLSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}