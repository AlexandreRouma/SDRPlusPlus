#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <options.h>
#include <libairspy/airspy.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "airspy_source",
    /* Description:     */ "Airspy source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class AirspySourceModule : public ModuleManager::Instance {
public:
    AirspySourceModule(std::string name) {
        this->name = name;

        sampleRate = 10000000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected; 
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();

        // Select device from config
        config.aquire();
        std::string devSerial = config.conf["device"];
        config.release();
        selectByString(devSerial);

        sigpath::sourceManager.registerSource("Airspy", &handler);
    }

    ~AirspySourceModule() {
        
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
        int n = airspy_list_devices(serials, 256);

        char buf[1024];
        for (int i = 0; i < n; i++) {
            sprintf(buf, "%016" PRIX64, serials[i]);
            devList.push_back(serials[i]);
            devListTxt += buf;
            devListTxt += '\0';
        }
    }

    void selectFirst() {
        if (devList.size() != 0) {
            selectBySerial(devList[0]);
        }
    }

    void selectByString(std::string serial) {
        char buf[1024];
        for (int i = 0; i < devList.size(); i++) {
            sprintf(buf, "%016" PRIX64, devList[i]);
            std::string str = buf;
            if (serial == str) {
                selectBySerial(devList[i]);
                return;
            }
        }
        selectFirst();
    }

    void selectBySerial(uint64_t serial) {
        selectedSerial = serial;
        airspy_device* dev;
        int err = airspy_open_sn(&dev, selectedSerial);
        if (err != 0) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, selectedSerial);
            spdlog::error("Could not open Airspy HF+ {0}", buf);
            return;
        }

        uint32_t sampleRates[256];
        airspy_get_samplerates(dev, sampleRates, 0);
        int n = sampleRates[0];
        airspy_get_samplerates(dev, sampleRates, n);
        char buf[1024];
        sampleRateList.clear();
        sampleRateListTxt = "";
        for (int i = 0; i < n; i++) {
            sampleRateList.push_back(sampleRates[i]);
            sprintf(buf, "%d", sampleRates[i]);
            sampleRateListTxt += buf;
            sampleRateListTxt += '\0';
        }

        sprintf(buf, "%016" PRIX64, serial);
        selectedSerStr = std::string(buf);

        // Load config here
        config.aquire();
        bool created = false;
        if (!config.conf["devices"].contains(selectedSerStr)) {
            created = true;
            config.conf["devices"][selectedSerStr]["sampleRate"] = 10000000;
            config.conf["devices"][selectedSerStr]["gainMode"] = 0;
            config.conf["devices"][selectedSerStr]["sensitiveGain"] = 0;
            config.conf["devices"][selectedSerStr]["linearGain"] = 0;
            config.conf["devices"][selectedSerStr]["lnaGain"] = 0;
            config.conf["devices"][selectedSerStr]["mixerGain"] = 0;
            config.conf["devices"][selectedSerStr]["vgaGain"] = 0;
            config.conf["devices"][selectedSerStr]["lnaAgc"] = false;
            config.conf["devices"][selectedSerStr]["mixerAgc"] = false;
            config.conf["devices"][selectedSerStr]["biasT"] = false;
        }

        // Load sample rate
        srId = 0;
        if (config.conf["devices"][selectedSerStr].contains("sampleRate")) {
            int selectedSr = config.conf["devices"][selectedSerStr]["sampleRate"];
            for (int i = 0; i < sampleRateList.size(); i++) {
                if (sampleRateList[i] == selectedSr) {
                    srId = i;
                    break;
                }
            }
        }

        // Load gains
        if (config.conf["devices"][selectedSerStr].contains("gainMode")) {
            gainMode = config.conf["devices"][selectedSerStr]["gainMode"];
        }
        if (config.conf["devices"][selectedSerStr].contains("sensitiveGain")) {
            sensitiveGain = config.conf["devices"][selectedSerStr]["sensitiveGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("linearGain")) {
            linearGain = config.conf["devices"][selectedSerStr]["linearGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("lnaGain")) {
            lnaGain = config.conf["devices"][selectedSerStr]["lnaGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("mixerGain")) {
            mixerGain = config.conf["devices"][selectedSerStr]["mixerGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("vgaGain")) {
            vgaGain = config.conf["devices"][selectedSerStr]["vgaGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("lnaAgc")) {
            lnaAgc = config.conf["devices"][selectedSerStr]["lnaAgc"];
        }
        if (config.conf["devices"][selectedSerStr].contains("mixerAgc")) {
            mixerAgc = config.conf["devices"][selectedSerStr]["mixerAgc"];
        }

        // Load Bias-T
        if (config.conf["devices"][selectedSerStr].contains("biasT")) {
            biasT = config.conf["devices"][selectedSerStr]["biasT"];
        }

        config.release(created);

        airspy_close(dev);
    }

private:
    static void menuSelected(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("AirspySourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        spdlog::info("AirspySourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        if (_this->running) {
            return;
        }
        if (_this->selectedSerial == 0) {
            spdlog::error("Tried to start AirspyHF+ source with null serial");
            return;
        }

        int err = airspy_open_sn(&_this->openDev, _this->selectedSerial);
        if (err != 0) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, _this->selectedSerial);
            spdlog::error("Could not open Airspy {0}", buf);
            return;
        }

        airspy_set_samplerate(_this->openDev, _this->sampleRateList[_this->srId]);
        airspy_set_freq(_this->openDev, _this->freq);
        
        if (_this->gainMode == 0) {
            airspy_set_lna_agc(_this->openDev, 0);
            airspy_set_mixer_agc(_this->openDev, 0);
            airspy_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
        }
        else if (_this->gainMode == 1) {
            airspy_set_lna_agc(_this->openDev, 0);
            airspy_set_mixer_agc(_this->openDev, 0);
            airspy_set_linearity_gain(_this->openDev, _this->linearGain);
        }
        else if (_this->gainMode == 2) {
            if (_this->lnaAgc) {
                airspy_set_lna_agc(_this->openDev, 1);
            }
            else {
                airspy_set_lna_agc(_this->openDev, 0);
                airspy_set_lna_gain(_this->openDev, _this->lnaGain);
            }
            if (_this->mixerAgc) {
                airspy_set_mixer_agc(_this->openDev, 1);
            }
            else {
                airspy_set_mixer_agc(_this->openDev, 0);
                airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
            }
            airspy_set_vga_gain(_this->openDev, _this->vgaGain);
        }

        airspy_set_rf_bias(_this->openDev, _this->biasT);

        airspy_start_rx(_this->openDev, callback, _this);

        _this->running = true;
        spdlog::info("AirspySourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->stream.stopWriter();
        airspy_close(_this->openDev);
        _this->stream.clearWriteStop();
        spdlog::info("AirspySourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        if (_this->running) {
            airspy_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        spdlog::info("AirspySourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_airspy_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectBySerial(_this->devList[_this->devId]);
            if (_this->selectedSerStr != "") {
                config.aquire();
                config.conf["device"] = _this->selectedSerStr;
                config.release(true);
            }
        }

        if (ImGui::Combo(CONCAT("##_airspy_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            _this->sampleRate = _this->sampleRateList[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.aquire();
                config.conf["devices"][_this->selectedSerStr]["sampleRate"] = _this->sampleRate;
                config.release(true);
            }
        }

        ImGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (ImGui::Button(CONCAT("Refresh##_airspy_refr_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
            config.aquire();
            std::string devSerial = config.conf["device"];
            config.release();
            _this->selectByString(devSerial);
        }

        if (_this->running) { style::endDisabled(); }

        ImGui::BeginGroup();
        ImGui::Columns(3, CONCAT("AirspyGainModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("Sensitive##_airspy_gm_", _this->name), _this->gainMode == 0)) {
            _this->gainMode = 0;
            if (_this->running) {
                airspy_set_lna_agc(_this->openDev, 0);
                airspy_set_mixer_agc(_this->openDev, 0);
                airspy_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
            }
            if (_this->selectedSerStr != "") {
                config.aquire();
                config.conf["devices"][_this->selectedSerStr]["gainMode"] = 0;
                config.release(true);
            }
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("Linear##_airspy_gm_", _this->name), _this->gainMode == 1)) {
            _this->gainMode = 1;
            if (_this->running) {
                airspy_set_lna_agc(_this->openDev, 0);
                airspy_set_mixer_agc(_this->openDev, 0);
                airspy_set_linearity_gain(_this->openDev, _this->linearGain);
            }
            if (_this->selectedSerStr != "") {
                config.aquire();
                config.conf["devices"][_this->selectedSerStr]["gainMode"] = 1;
                config.release(true);
            }
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("Free##_airspy_gm_", _this->name), _this->gainMode == 2)) {
            _this->gainMode = 2;
            if (_this->running) {
                if (_this->lnaAgc) {
                    airspy_set_lna_agc(_this->openDev, 1);
                }
                else {
                    airspy_set_lna_agc(_this->openDev, 0);
                    airspy_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                if (_this->mixerAgc) {
                    airspy_set_mixer_agc(_this->openDev, 1);
                }
                else {
                    airspy_set_mixer_agc(_this->openDev, 0);
                    airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
                }
                airspy_set_vga_gain(_this->openDev, _this->vgaGain);
            }
            if (_this->selectedSerStr != "") {
                config.aquire();
                config.conf["devices"][_this->selectedSerStr]["gainMode"] = 2;
                config.release(true);
            }
        }
        ImGui::Columns(1, CONCAT("EndAirspyGainModeColumns##_", _this->name), false);
        ImGui::EndGroup();

        // Gain menus

        if (_this->gainMode == 0) {
            ImGui::Text("Gain");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderInt(CONCAT("##_airspy_sens_gain_", _this->name), &_this->sensitiveGain, 0, 21)) {
                if (_this->running) {
                    airspy_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
                }
                if (_this->selectedSerStr != "") {
                    config.aquire();
                    config.conf["devices"][_this->selectedSerStr]["sensitiveGain"] = _this->sensitiveGain;
                    config.release(true);
                }
            }
        }
        else if (_this->gainMode == 1) {
            ImGui::Text("Gain");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderInt(CONCAT("##_airspy_lin_gain_", _this->name), &_this->linearGain, 0, 21)) {
                if (_this->running) {
                    airspy_set_linearity_gain(_this->openDev, _this->linearGain);
                }
                if (_this->selectedSerStr != "") {
                    config.aquire();
                    config.conf["devices"][_this->selectedSerStr]["linearGain"] = _this->linearGain;
                    config.release(true);
                }
            }
        }
        else if (_this->gainMode == 2) {
            if (_this->lnaAgc) { style::beginDisabled(); }
            ImGui::Text("LNA Gain");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderInt(CONCAT("##_airspy_lna_gain_", _this->name), &_this->lnaGain, 0, 15)) {
                if (_this->running) {
                    airspy_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                if (_this->selectedSerStr != "") {
                    config.aquire();
                    config.conf["devices"][_this->selectedSerStr]["lnaGain"] = _this->lnaGain;
                    config.release(true);
                }
            }
            if (_this->lnaAgc) { style::endDisabled(); }

            if (_this->mixerAgc) { style::beginDisabled(); }
            ImGui::Text("Mixer Gain");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderInt(CONCAT("##_airspy_mix_gain_", _this->name), &_this->mixerGain, 0, 15)) {
                if (_this->running) {
                    airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
                }
                if (_this->selectedSerStr != "") {
                    config.aquire();
                    config.conf["devices"][_this->selectedSerStr]["mixerGain"] = _this->mixerGain;
                    config.release(true);
                }
            }
            if (_this->mixerAgc) { style::endDisabled(); }

            ImGui::Text("VGA Gain");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderInt(CONCAT("##_airspy_vga_gain_", _this->name), &_this->vgaGain, 0, 15)) {
                if (_this->running) {
                    airspy_set_vga_gain(_this->openDev, _this->vgaGain);
                }
                if (_this->selectedSerStr != "") {
                    config.aquire();
                    config.conf["devices"][_this->selectedSerStr]["vgaGain"] = _this->vgaGain;
                    config.release(true);
                }
            }

            // AGC Control
            if (ImGui::Checkbox(CONCAT("LNA AGC##_airspy_", _this->name), &_this->lnaAgc)) {
                if (_this->running) {
                    if (_this->lnaAgc) {
                        airspy_set_lna_agc(_this->openDev, 1);
                    }
                    else {
                        airspy_set_lna_agc(_this->openDev, 0);
                        airspy_set_lna_gain(_this->openDev, _this->lnaGain);
                    }
                }
                if (_this->selectedSerStr != "") {
                    config.aquire();
                    config.conf["devices"][_this->selectedSerStr]["lnaAgc"] = _this->lnaAgc;
                    config.release(true);
                }
            }

            if (ImGui::Checkbox(CONCAT("Mixer AGC##_airspy_", _this->name), &_this->mixerAgc)) {
                if (_this->running) {
                    if (_this->mixerAgc) {
                        airspy_set_mixer_agc(_this->openDev, 1);
                    }
                    else {
                        airspy_set_mixer_agc(_this->openDev, 0);
                        airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
                    }
                }
                if (_this->selectedSerStr != "") {
                    config.aquire();
                    config.conf["devices"][_this->selectedSerStr]["mixerAgc"] = _this->mixerAgc;
                    config.release(true);
                }
            }
        }

        // Bias T

        if (ImGui::Checkbox(CONCAT("Bias T##_airspy_", _this->name), &_this->biasT)) {
            if (_this->running) {
                airspy_set_rf_bias(_this->openDev, _this->biasT);
            }
            if (_this->selectedSerStr != "") {
                config.aquire();
                config.conf["devices"][_this->selectedSerStr]["biasT"] = _this->biasT;
                config.release(true);
            }
        }

        
    }

    static int callback(airspy_transfer_t* transfer) {
        AirspySourceModule* _this = (AirspySourceModule*)transfer->ctx;
        memcpy(_this->stream.writeBuf, transfer->samples, transfer->sample_count * sizeof(dsp::complex_t));
        if (!_this->stream.swap(transfer->sample_count)) { return -1; }
        return 0;
    }

    std::string name;
    airspy_device* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    uint64_t selectedSerial = 0;
    std::string selectedSerStr = "";
    int devId = 0;
    int srId = 0;

    bool biasT = false;

    int lnaGain = 0;
    int vgaGain = 0;
    int mixerGain = 0;
    int linearGain = 0;
    int sensitiveGain = 0;

    int gainMode = 0;

    bool lnaAgc = false;
    bool mixerAgc = false;

    std::vector<uint64_t> devList;
    std::string devListTxt;
    std::vector<uint32_t> sampleRateList;
    std::string sampleRateListTxt;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(options::opts.root + "/airspy_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new AirspySourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (AirspySourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}