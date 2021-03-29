#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <libhackrf/hackrf.h>

#pragma optimize( "", off )

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "hackrf_source",
    /* Description:     */ "HackRF source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

//ConfigManager config;

const char* AGG_MODES_STR = "Off\0Low\0High\0";

const char* sampleRatesTxt = "20MHz\00016MHz\00010MHz\0008MHz\0005MHz\0004MHz\0002MHz\000";

const int sampleRates[] = {
    20000000,
    16000000,
    10000000,
    8000000,
    5000000,
    4000000,
    2000000,
};

class HackRFSourceModule : public ModuleManager::Instance {
public:
    HackRFSourceModule(std::string name) {
        this->name = name;

        hackrf_init();

        sampleRate = 2000000;

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

        // config.aquire();
        // std::string serString = config.conf["device"];
        // config.release();

        sigpath::sourceManager.registerSource("HackRF", &handler);
    }

    ~HackRFSourceModule() {
        hackrf_exit();
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
        devList.clear();
        devListTxt = "";

        uint64_t serials[256];
        hackrf_device_list_t* _devList = hackrf_device_list();

        for (int i = 0; i < _devList->devicecount; i++) {
            devList.push_back(_devList->serial_numbers[i]);
            devListTxt += (char*)(_devList->serial_numbers[i] + 16);
            devListTxt += '\0';
        }

        hackrf_device_list_free(_devList);
    }

    void selectFirst() {
        if (devList.size() != 0) {
            selectedSerial = devList[0];
        }
    }

private:
    static void menuSelected(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("HackRFSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        spdlog::info("HackRFSourceModule '{0}': Menu Deselect!", _this->name);
    }

    
    
    static void start(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        if (_this->running) {
            return;
        }
        if (_this->selectedSerial == "") {
            spdlog::error("Tried to start HackRF source with empty serial");
            return;
        }

        int err = hackrf_open_by_serial(_this->selectedSerial.c_str(), &_this->openDev);
        if (err != 0) {
            spdlog::error("Could not open HackRF {0}", _this->selectedSerial);
            return;
        }

        hackrf_set_sample_rate(_this->openDev, _this->sampleRate);
        hackrf_set_baseband_filter_bandwidth(_this->openDev, hackrf_compute_baseband_filter_bw(_this->sampleRate));
        hackrf_set_freq(_this->openDev, _this->freq);

        hackrf_set_amp_enable(_this->openDev, _this->amp);
        hackrf_set_lna_gain(_this->openDev, _this->lna);
        hackrf_set_vga_gain(_this->openDev, _this->lna);

        hackrf_start_rx(_this->openDev, callback, _this);

        _this->running = true;
        spdlog::info("HackRFSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->stream.stopWriter();
        // TODO: Stream stop
        hackrf_close(_this->openDev);
        _this->stream.clearWriteStop();
        spdlog::info("HackRFSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        if (_this->running) {
            hackrf_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        spdlog::info("HackRFSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_hackrf_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectedSerial = _this->devList[_this->devId];
        }

        if (ImGui::Combo(CONCAT("##_hackrf_sr_sel_", _this->name), &_this->srId, sampleRatesTxt)) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
        }

        ImGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (ImGui::Button(CONCAT("Refresh##_hackrf_refr_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
        }

        if (_this->running) { style::endDisabled(); }

        ImGui::Text("Amp Enabled");
        ImGui::SameLine();
        if (ImGui::Checkbox(CONCAT("##_hackrf_amp_", _this->name), &_this->amp)) {
            if (_this->running) {
                hackrf_set_amp_enable(_this->openDev, _this->amp);
            }
        }

        ImGui::Text("LNA Gain");
        ImGui::SameLine();
        if (ImGui::SliderInt(CONCAT("##_hackrf_lna_", _this->name), &_this->lna, 0, 40)) {
            _this->lna = (_this->lna / 8) * 8;
            if (_this->running) {
                hackrf_set_lna_gain(_this->openDev, _this->lna);
            }
        }

        ImGui::Text("LNA Gain");
        ImGui::SameLine();
        if (ImGui::SliderInt(CONCAT("##_hackrf_vga_", _this->name), &_this->vga, 0, 62)) {
            _this->vga = (_this->vga / 2) * 2;
            if (_this->running) {
                hackrf_set_vga_gain(_this->openDev, _this->lna);
            }
        }   
    }

    static int callback(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->rx_ctx;
        int count = transfer->valid_length / 2;
        int8_t* buffer = (int8_t*)transfer->buffer;
        for (int i = 0; i < count; i++) {
            _this->stream.writeBuf[i].re = (float)buffer[i * 2] / 128.0f;
            _this->stream.writeBuf[i].im = (float)buffer[(i * 2) + 1] / 128.0f;
        }
        if (!_this->stream.swap(count)) { return -1; }
        return 0;
    }

    std::string name;
    hackrf_device* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    int sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    std::string selectedSerial = "";
    int devId = 0;
    int srId = 0;
    bool amp = false;
    int lna = 0;
    int vga = 0;

    std::vector<std::string> devList;
    std::string devListTxt;
};

MOD_EXPORT void _INIT_() {
//    config.setPath(ROOT_DIR "/airspyhf_config.json");
//    json defConf;
//    defConf["device"] = "";
//    defConf["devices"] = json::object();
//    config.load(defConf);
//    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new HackRFSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (HackRFSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // config.disableAutoSave();
    // config.save();
}

#pragma optimize( "", on )