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
#include <libsddc.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "sddc_source",
    /* Description:     */ "SDDC source module for SDR++",
    /* Author:          */ "Ryzerth;pkuznetsov",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const char* AGG_MODES_STR = "Off\0Low\0High\0";

class AirspyHFSourceModule : public ModuleManager::Instance {
public:
    AirspyHFSourceModule(std::string name) {
        this->name = name;

        if (options::opts.serverMode) { return; }

        sampleRate = 768000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();

        selectFirst();

        sigpath::sourceManager.registerSource("SDDC", &handler);
    }

    ~AirspyHFSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("SDDC");
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

    void refresh() {
        devListTxt = "";

        devCount = sddc_get_device_count();
        for (int i = 0; i < devCount; i++) {
            // Open device
            sddc_t* dev = sddc_open(i, "../sddc_source/res/firmwares/SDDC_FX3.img");
            if (dev == NULL) { continue; }

            // Get device name (check if implemented)
            const char* name = sddc_get_hw_model_name(dev);
            if (name == NULL) {
                sddc_close(dev);
                continue;
            }

            // Add to list
            char tmp[256];
            sprintf(tmp, "%s (%d)", name, i);
            devNames.push_back(name);
            devListTxt += name;
            devListTxt += '\0';
            sddc_close(dev);
        }
    }

    void selectFirst() {
        if (devCount != 0) {
            selectById(0);
        }
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devCount; i++) {
            if (devNames[i] == name) {
                selectById(i);
                break;
            }
        }
    }

    void selectById(int id) {
        if (id < 0 || id >= devCount) {
            selectedDevName = "";
            return;
        }

        devId = id;
        selectedDevName = devNames[id];
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
        spdlog::info("AirspyHFSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        spdlog::info("AirspyHFSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->selectedDevName == "") { return; }

        // Start device

        _this->running = true;
        spdlog::info("AirspyHFSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();

        // Stop device

        _this->stream.clearWriteStop();
        spdlog::info("AirspyHFSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (_this->running) {
            // Tune device
        }
        _this->freq = freq;
        spdlog::info("AirspyHFSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_sddc_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            // Select here
        }

        if (ImGui::Combo(CONCAT("##_sddc_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            _this->sampleRate = _this->sampleRateList[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            // Select SR here
        }

        ImGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (ImGui::Button(CONCAT("Refresh##_sddc_refr_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
            // Reselect and reset samplerate if it changed
        }

        if (_this->running) { style::endDisabled(); }

        // All other controls
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    int devId = 0;
    int srId = 0;
    sddc_t* openDev;

    int devCount = 0;
    std::vector<std::string> devNames;
    std::string selectedDevName = "";
    std::string devListTxt;
    std::vector<uint32_t> sampleRateList;
    std::string sampleRateListTxt;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(options::opts.root + "/sddc_config.json");
    config.load(def);
    config.enableAutoSave();
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