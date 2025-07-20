#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <hydrasdr.h>
#include <utils/optionlist.h>

#ifdef __ANDROID__
#include <android_backend.h>
#endif

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "hydrasdr_source",
    /* Description:     */ "HydraSDR source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class HydraSDRSourceModule : public ModuleManager::Instance {
public:
    HydraSDRSourceModule(std::string name) {
        this->name = name;

        // Define the ports
        ports.define("rx0", "RX0", RF_PORT_RX0);
        ports.define("rx1", "RX1", RF_PORT_RX1);
        ports.define("rx2", "RX2", RF_PORT_RX2);

        regStr[0] = 0;
        valStr[0] = 0;

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
        config.acquire();
        std::string devSerial = config.conf["device"];
        config.release();
        selectByString(devSerial);

        sigpath::sourceManager.registerSource("HydraSDR", &handler);
    }

    ~HydraSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("HydraSDR");;
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
#ifndef __ANDROID__
        devices.clear();

        uint64_t serials[256];
        int n = hydrasdr_list_devices(serials, 256);

        char buf[1024];
        for (int i = 0; i < n; i++) {
            sprintf(buf, "%016" PRIX64, serials[i]);
            devices.define(buf, buf, serials[i]);
        }
#else
        // Check for device presence
        int vid, pid;
        devFd = backend::getDeviceFD(vid, pid, backend::AIRSPY_VIDPIDS);
        if (devFd < 0) { return; }

        // Get device info
        std::string fakeName = "HydraSDR USB";
        devList.push_back(0xDEADBEEF);
        devListTxt += fakeName;
        devListTxt += '\0';
#endif
    }

    void selectFirst() {
        if (!devices.empty()) {
            selectBySerial(devices.value(0));
        }
    }

    void selectByString(std::string serial) {
        if (devices.keyExists(serial)) {
            selectBySerial(devices.value(devices.keyId(serial)));
            return;
        }
        selectFirst();
    }

    void selectBySerial(uint64_t serial) {
        hydrasdr_device* dev;
        try {
#ifndef __ANDROID__
            int err = hydrasdr_open_sn(&dev, serial);
#else
            int err = hydrasdr_open_fd(&dev, devFd);
#endif
            if (err != 0) {
                char buf[1024];
                sprintf(buf, "%016" PRIX64, serial);
                flog::error("Could not open HydraSDR {0}", buf);
                selectedSerial = 0;
                return;
            }
        }
        catch (const std::exception& e) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, serial);
            flog::error("Could not open HydraSDR {}", buf);
        }
        devId = devices.valueId(serial);
        selectedSerial = serial;
        selectedSerStr = devices.key(devId);

        uint32_t sampleRates[256];
        hydrasdr_get_samplerates(dev, sampleRates, 0);
        int n = sampleRates[0];
        hydrasdr_get_samplerates(dev, sampleRates, n);
        samplerates.clear();
        for (int i = 0; i < n; i++) {
            samplerates.define(sampleRates[i], getBandwdithScaled(sampleRates[i]), sampleRates[i]);
        }

        // Load config here
        config.acquire();
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
        sampleRate = samplerates.value(0);
        if (config.conf["devices"][selectedSerStr].contains("sampleRate")) {
            int selectedSr = config.conf["devices"][selectedSerStr]["sampleRate"];
            if (samplerates.keyExists(selectedSr)) {
                srId = samplerates.keyId(selectedSr);
                sampleRate = samplerates[srId];
            }
        }

        // Load port
        if (config.conf["devices"][selectedSerStr].contains("port")) {
            std::string portStr = config.conf["devices"][selectedSerStr]["port"];
            if (ports.keyExists(portStr)) {
                portId = ports.keyId(portStr);
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

        hydrasdr_close(dev);
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
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("HydraSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        flog::info("HydraSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->selectedSerial == 0) {
            flog::error("Tried to start HydraSDR source with null serial");
            return;
        }

#ifndef __ANDROID__
        int err = hydrasdr_open_sn(&_this->openDev, _this->selectedSerial);
#else
        int err = hydrasdr_open_fd(&_this->openDev, _this->devFd);
#endif
        if (err != 0) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, _this->selectedSerial);
            flog::error("Could not open HydraSDR {0}", buf);
            return;
        }

        hydrasdr_set_samplerate(_this->openDev, _this->samplerates[_this->srId]);
        hydrasdr_set_freq(_this->openDev, _this->freq);

        hydrasdr_set_rf_port(_this->openDev, _this->ports[_this->portId]);

        if (_this->gainMode == 0) {
            hydrasdr_set_lna_agc(_this->openDev, 0);
            hydrasdr_set_mixer_agc(_this->openDev, 0);
            hydrasdr_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
        }
        else if (_this->gainMode == 1) {
            hydrasdr_set_lna_agc(_this->openDev, 0);
            hydrasdr_set_mixer_agc(_this->openDev, 0);
            hydrasdr_set_linearity_gain(_this->openDev, _this->linearGain);
        }
        else if (_this->gainMode == 2) {
            if (_this->lnaAgc) {
                hydrasdr_set_lna_agc(_this->openDev, 1);
            }
            else {
                hydrasdr_set_lna_agc(_this->openDev, 0);
                hydrasdr_set_lna_gain(_this->openDev, _this->lnaGain);
            }
            if (_this->mixerAgc) {
                hydrasdr_set_mixer_agc(_this->openDev, 1);
            }
            else {
                hydrasdr_set_mixer_agc(_this->openDev, 0);
                hydrasdr_set_mixer_gain(_this->openDev, _this->mixerGain);
            }
            hydrasdr_set_vga_gain(_this->openDev, _this->vgaGain);
        }

        hydrasdr_set_rf_bias(_this->openDev, _this->biasT);

        hydrasdr_start_rx(_this->openDev, callback, _this);

        _this->running = true;
        flog::info("HydraSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        hydrasdr_close(_this->openDev);
        _this->stream.clearWriteStop();
        flog::info("HydraSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        if (_this->running) {
            hydrasdr_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        flog::info("HydraSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_hydrasdr_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->selectBySerial(_this->devices[_this->devId]);
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["device"] = _this->selectedSerStr;
                config.release(true);
            }
        }

        if (SmGui::Combo(CONCAT("##_hydrasdr_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["sampleRate"] = _this->samplerates.key(_this->srId);
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_hydrasdr_refr_", _this->name))) {
            _this->refresh();
            config.acquire();
            std::string devSerial = config.conf["device"];
            config.release();
            _this->selectByString(devSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Antenna Port");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_hydrasdr_port_", _this->name), &_this->portId, _this->ports.txt)) {
            if (_this->running) {
                hydrasdr_set_rf_port(_this->openDev, _this->ports[_this->portId]);
            }
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["port"] = _this->ports.key(_this->portId);
                config.release(true);
            }
        }

        SmGui::BeginGroup();
        SmGui::Columns(3, CONCAT("HydraSDRGainModeColumns##_", _this->name), false);
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Sensitive##_hydrasdr_gm_", _this->name), _this->gainMode == 0)) {
            _this->gainMode = 0;
            if (_this->running) {
                hydrasdr_set_lna_agc(_this->openDev, 0);
                hydrasdr_set_mixer_agc(_this->openDev, 0);
                hydrasdr_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
            }
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["gainMode"] = 0;
                config.release(true);
            }
        }
        SmGui::NextColumn();
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Linear##_hydrasdr_gm_", _this->name), _this->gainMode == 1)) {
            _this->gainMode = 1;
            if (_this->running) {
                hydrasdr_set_lna_agc(_this->openDev, 0);
                hydrasdr_set_mixer_agc(_this->openDev, 0);
                hydrasdr_set_linearity_gain(_this->openDev, _this->linearGain);
            }
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["gainMode"] = 1;
                config.release(true);
            }
        }
        SmGui::NextColumn();
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Free##_hydrasdr_gm_", _this->name), _this->gainMode == 2)) {
            _this->gainMode = 2;
            if (_this->running) {
                if (_this->lnaAgc) {
                    hydrasdr_set_lna_agc(_this->openDev, 1);
                }
                else {
                    hydrasdr_set_lna_agc(_this->openDev, 0);
                    hydrasdr_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                if (_this->mixerAgc) {
                    hydrasdr_set_mixer_agc(_this->openDev, 1);
                }
                else {
                    hydrasdr_set_mixer_agc(_this->openDev, 0);
                    hydrasdr_set_mixer_gain(_this->openDev, _this->mixerGain);
                }
                hydrasdr_set_vga_gain(_this->openDev, _this->vgaGain);
            }
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["gainMode"] = 2;
                config.release(true);
            }
        }
        SmGui::Columns(1, CONCAT("EndHydraSDRGainModeColumns##_", _this->name), false);
        SmGui::EndGroup();

        // Gain menus

        if (_this->gainMode == 0) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_hydrasdr_sens_gain_", _this->name), &_this->sensitiveGain, 0, 21)) {
                if (_this->running) {
                    hydrasdr_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["sensitiveGain"] = _this->sensitiveGain;
                    config.release(true);
                }
            }
        }
        else if (_this->gainMode == 1) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_hydrasdr_lin_gain_", _this->name), &_this->linearGain, 0, 21)) {
                if (_this->running) {
                    hydrasdr_set_linearity_gain(_this->openDev, _this->linearGain);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["linearGain"] = _this->linearGain;
                    config.release(true);
                }
            }
        }
        else if (_this->gainMode == 2) {
            // TODO: Switch to a table for alignment
            if (_this->lnaAgc) { SmGui::BeginDisabled(); }
            SmGui::LeftLabel("LNA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_hydrasdr_lna_gain_", _this->name), &_this->lnaGain, 0, 15)) {
                if (_this->running) {
                    hydrasdr_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["lnaGain"] = _this->lnaGain;
                    config.release(true);
                }
            }
            if (_this->lnaAgc) { SmGui::EndDisabled(); }

            if (_this->mixerAgc) { SmGui::BeginDisabled(); }
            SmGui::LeftLabel("Mixer Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_hydrasdr_mix_gain_", _this->name), &_this->mixerGain, 0, 15)) {
                if (_this->running) {
                    hydrasdr_set_mixer_gain(_this->openDev, _this->mixerGain);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["mixerGain"] = _this->mixerGain;
                    config.release(true);
                }
            }
            if (_this->mixerAgc) { SmGui::EndDisabled(); }

            SmGui::LeftLabel("VGA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_hydrasdr_vga_gain_", _this->name), &_this->vgaGain, 0, 15)) {
                if (_this->running) {
                    hydrasdr_set_vga_gain(_this->openDev, _this->vgaGain);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["vgaGain"] = _this->vgaGain;
                    config.release(true);
                }
            }

            // AGC Control
            SmGui::ForceSync();
            if (SmGui::Checkbox(CONCAT("LNA AGC##_hydrasdr_", _this->name), &_this->lnaAgc)) {
                if (_this->running) {
                    if (_this->lnaAgc) {
                        hydrasdr_set_lna_agc(_this->openDev, 1);
                    }
                    else {
                        hydrasdr_set_lna_agc(_this->openDev, 0);
                        hydrasdr_set_lna_gain(_this->openDev, _this->lnaGain);
                    }
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["lnaAgc"] = _this->lnaAgc;
                    config.release(true);
                }
            }
            SmGui::ForceSync();
            if (SmGui::Checkbox(CONCAT("Mixer AGC##_hydrasdr_", _this->name), &_this->mixerAgc)) {
                if (_this->running) {
                    if (_this->mixerAgc) {
                        hydrasdr_set_mixer_agc(_this->openDev, 1);
                    }
                    else {
                        hydrasdr_set_mixer_agc(_this->openDev, 0);
                        hydrasdr_set_mixer_gain(_this->openDev, _this->mixerGain);
                    }
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["mixerAgc"] = _this->mixerAgc;
                    config.release(true);
                }
            }
        }

        // Bias T
        if (SmGui::Checkbox(CONCAT("Bias T##_hydrasdr_", _this->name), &_this->biasT)) {
            if (_this->running) {
                hydrasdr_set_rf_bias(_this->openDev, _this->biasT);
            }
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["biasT"] = _this->biasT;
                config.release(true);
            }
        }

        SmGui::LeftLabel("Reg");
        SmGui::FillWidth();
        SmGui::InputText(CONCAT("##_badgesdr_reg_", _this->name), _this->regStr, 256);
        SmGui::LeftLabel("Value");
        SmGui::FillWidth();
        SmGui::InputText(CONCAT("##_badgesdr_val_", _this->name), _this->valStr, 256);
        SmGui::FillWidth();
        if (ImGui::Button(CONCAT("Read##_badgesdr_rd_", _this->name))) {
            if (_this->running) {
                uint8_t val;
                hydrasdr_r82x_read(_this->openDev, std::stoi(_this->regStr, NULL, 16), &val);
                sprintf(_this->valStr, "%02X", val);
            }
        }
        SmGui::FillWidth();
        if (ImGui::Button(CONCAT("Write##_badgesdr_wr_", _this->name))) {
            if (_this->running) {
                hydrasdr_r82x_write(_this->openDev, std::stoi(_this->regStr, NULL, 16), std::stoi(_this->valStr, NULL, 16));
            }
        }
    }

    char valStr[256];
    char regStr[256];

    static int callback(hydrasdr_transfer_t* transfer) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)transfer->ctx;
        memcpy(_this->stream.writeBuf, transfer->samples, transfer->sample_count * sizeof(dsp::complex_t));
        if (!_this->stream.swap(transfer->sample_count)) { return -1; }
        return 0;
    }

    std::string name;
    hydrasdr_device* openDev;
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
    int portId = 0;

    bool biasT = false;

    int lnaGain = 0;
    int vgaGain = 0;
    int mixerGain = 0;
    int linearGain = 0;
    int sensitiveGain = 0;

    int gainMode = 0;

    bool lnaAgc = false;
    bool mixerAgc = false;

#ifdef __ANDROID__
    int devFd = 0;
#endif

    OptionList<std::string, uint64_t> devices;
    OptionList<uint32_t, uint32_t> samplerates;
    OptionList<std::string, hydrasdr_rf_port_t> ports;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/hydrasdr_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new HydraSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (HydraSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}