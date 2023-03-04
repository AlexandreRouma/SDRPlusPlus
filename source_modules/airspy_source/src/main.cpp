#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <airspy.h>

#ifdef __ANDROID__
#include <android_backend.h>
#endif

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "airspy_source",
    /* Description:     */ "Airspy source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class AirspySourceModule : public ModuleManager::Instance, public Source {
public:
    AirspySourceModule(std::string name) {
        this->name = name;

        airspy_init();

        sampleRate = 10000000.0;

        refresh();
        if (sampleRateList.size() > 0) {
            sampleRate = sampleRateList[0];
        }

        // Select device from config
        config.acquire();
        std::string devSerial = config.conf["device"];
        config.release();
        selectByString(devSerial);

        sigpath::sourceManager.registerSource("Airspy", this);
    }

    ~AirspySourceModule() {
        stop();
        sigpath::sourceManager.unregisterSource("Airspy");
        airspy_exit();
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
#else
        // Check for device presence
        int vid, pid;
        devFd = backend::getDeviceFD(vid, pid, backend::AIRSPY_VIDPIDS);
        if (devFd < 0) { return; }

        // Get device info
        std::string fakeName = "Airspy USB";
        devList.push_back(0xDEADBEEF);
        devListTxt += fakeName;
        devListTxt += '\0';
#endif
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
        airspy_device* dev;
        try {
#ifndef __ANDROID__
            int err = airspy_open_sn(&dev, serial);
#else
            int err = airspy_open_fd(&dev, devFd);
#endif
            if (err != 0) {
                char buf[1024];
                sprintf(buf, "%016" PRIX64, serial);
                flog::error("Could not open Airspy {0}", buf);
                selectedSerial = 0;
                return;
            }
        }
        catch (std::exception e) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, serial);
            flog::error("Could not open Airspy {0}", buf);
        }
        selectedSerial = serial;

        uint32_t sampleRates[256];
        airspy_get_samplerates(dev, sampleRates, 0);
        int n = sampleRates[0];
        airspy_get_samplerates(dev, sampleRates, n);
        sampleRateList.clear();
        sampleRateListTxt = "";
        for (int i = 0; i < n; i++) {
            sampleRateList.push_back(sampleRates[i]);
            sampleRateListTxt += getBandwdithScaled(sampleRates[i]);
            sampleRateListTxt += '\0';
        }

        char buf[1024];
        sprintf(buf, "%016" PRIX64, serial);
        selectedSerStr = std::string(buf);

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
        sampleRate = sampleRateList[0];
        if (config.conf["devices"][selectedSerStr].contains("sampleRate")) {
            int selectedSr = config.conf["devices"][selectedSerStr]["sampleRate"];
            for (int i = 0; i < sampleRateList.size(); i++) {
                if (sampleRateList[i] == selectedSr) {
                    srId = i;
                    sampleRate = selectedSr;
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

        void select() {
        core::setInputSampleRate(sampleRate);
        flog::info("AirspySourceModule '{0}': Select!", name);
    }

    void deselect() {
        flog::info("AirspySourceModule '{0}': Deselect!", name);
    }

    bool start() {
        if (running) { return true; }
        if (selectedSerial == 0) {
            flog::error("Tried to start Airspy source with null serial");
            return false;
        }

#ifndef __ANDROID__
        int err = airspy_open_sn(&openDev, selectedSerial);
#else
        int err = airspy_open_fd(&openDev, devFd);
#endif
        if (err != 0) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, selectedSerial);
            flog::error("Could not open Airspy {0}", buf);
            return false;
        }

        airspy_set_samplerate(openDev, sampleRateList[srId]);
        airspy_set_freq(openDev, freq);

        if (gainMode == 0) {
            airspy_set_lna_agc(openDev, 0);
            airspy_set_mixer_agc(openDev, 0);
            airspy_set_sensitivity_gain(openDev, sensitiveGain);
        }
        else if (gainMode == 1) {
            airspy_set_lna_agc(openDev, 0);
            airspy_set_mixer_agc(openDev, 0);
            airspy_set_linearity_gain(openDev, linearGain);
        }
        else if (gainMode == 2) {
            if (lnaAgc) {
                airspy_set_lna_agc(openDev, 1);
            }
            else {
                airspy_set_lna_agc(openDev, 0);
                airspy_set_lna_gain(openDev, lnaGain);
            }
            if (mixerAgc) {
                airspy_set_mixer_agc(openDev, 1);
            }
            else {
                airspy_set_mixer_agc(openDev, 0);
                airspy_set_mixer_gain(openDev, mixerGain);
            }
            airspy_set_vga_gain(openDev, vgaGain);
        }

        airspy_set_rf_bias(openDev, biasT);

        airspy_start_rx(openDev, callback, this);

        running = true;
        flog::info("AirspySourceModule '{0}': Start!", name);
    }

    void stop() {
        if (!running) { return; }
        running = false;
        stream.stopWriter();
        airspy_close(openDev);
        stream.clearWriteStop();
        flog::info("AirspySourceModule '{0}': Stop!", name);
    }

    void tune(double freq) {
        this->freq = freq;
        if (running) {
            airspy_set_freq(openDev, freq);
        }
        flog::info("AirspySourceModule '{0}': Tune: {1}!", name, freq);
    }

    void showMenu() {
        if (running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_airspy_dev_sel_", name), &devId, devListTxt.c_str())) {
            selectBySerial(devList[devId]);
            core::setInputSampleRate(sampleRate);
            if (selectedSerStr != "") {
                config.acquire();
                config.conf["device"] = selectedSerStr;
                config.release(true);
            }
        }

        if (SmGui::Combo(CONCAT("##_airspy_sr_sel_", name), &srId, sampleRateListTxt.c_str())) {
            sampleRate = sampleRateList[srId];
            core::setInputSampleRate(sampleRate);
            if (selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][selectedSerStr]["sampleRate"] = sampleRate;
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_airspy_refr_", name))) {
            refresh();
            config.acquire();
            std::string devSerial = config.conf["device"];
            config.release();
            selectByString(devSerial);
            core::setInputSampleRate(sampleRate);
        }

        if (running) { SmGui::EndDisabled(); }

        SmGui::BeginGroup();
        SmGui::Columns(3, CONCAT("AirspyGainModeColumns##_", name), false);
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Sensitive##_airspy_gm_", name), gainMode == 0)) {
            gainMode = 0;
            if (running) {
                airspy_set_lna_agc(openDev, 0);
                airspy_set_mixer_agc(openDev, 0);
                airspy_set_sensitivity_gain(openDev, sensitiveGain);
            }
            if (selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][selectedSerStr]["gainMode"] = 0;
                config.release(true);
            }
        }
        SmGui::NextColumn();
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Linear##_airspy_gm_", name), gainMode == 1)) {
            gainMode = 1;
            if (running) {
                airspy_set_lna_agc(openDev, 0);
                airspy_set_mixer_agc(openDev, 0);
                airspy_set_linearity_gain(openDev, linearGain);
            }
            if (selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][selectedSerStr]["gainMode"] = 1;
                config.release(true);
            }
        }
        SmGui::NextColumn();
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Free##_airspy_gm_", name), gainMode == 2)) {
            gainMode = 2;
            if (running) {
                if (lnaAgc) {
                    airspy_set_lna_agc(openDev, 1);
                }
                else {
                    airspy_set_lna_agc(openDev, 0);
                    airspy_set_lna_gain(openDev, lnaGain);
                }
                if (mixerAgc) {
                    airspy_set_mixer_agc(openDev, 1);
                }
                else {
                    airspy_set_mixer_agc(openDev, 0);
                    airspy_set_mixer_gain(openDev, mixerGain);
                }
                airspy_set_vga_gain(openDev, vgaGain);
            }
            if (selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][selectedSerStr]["gainMode"] = 2;
                config.release(true);
            }
        }
        SmGui::Columns(1, CONCAT("EndAirspyGainModeColumns##_", name), false);
        SmGui::EndGroup();

        // Gain menus

        if (gainMode == 0) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_sens_gain_", name), &sensitiveGain, 0, 21)) {
                if (running) {
                    airspy_set_sensitivity_gain(openDev, sensitiveGain);
                }
                if (selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][selectedSerStr]["sensitiveGain"] = sensitiveGain;
                    config.release(true);
                }
            }
        }
        else if (gainMode == 1) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_lin_gain_", name), &linearGain, 0, 21)) {
                if (running) {
                    airspy_set_linearity_gain(openDev, linearGain);
                }
                if (selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][selectedSerStr]["linearGain"] = linearGain;
                    config.release(true);
                }
            }
        }
        else if (gainMode == 2) {
            // TODO: Switch to a table for alignment
            if (lnaAgc) { SmGui::BeginDisabled(); }
            SmGui::LeftLabel("LNA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_lna_gain_", name), &lnaGain, 0, 15)) {
                if (running) {
                    airspy_set_lna_gain(openDev, lnaGain);
                }
                if (selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][selectedSerStr]["lnaGain"] = lnaGain;
                    config.release(true);
                }
            }
            if (lnaAgc) { SmGui::EndDisabled(); }

            if (mixerAgc) { SmGui::BeginDisabled(); }
            SmGui::LeftLabel("Mixer Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_mix_gain_", name), &mixerGain, 0, 15)) {
                if (running) {
                    airspy_set_mixer_gain(openDev, mixerGain);
                }
                if (selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][selectedSerStr]["mixerGain"] = mixerGain;
                    config.release(true);
                }
            }
            if (mixerAgc) { SmGui::EndDisabled(); }

            SmGui::LeftLabel("VGA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_vga_gain_", name), &vgaGain, 0, 15)) {
                if (running) {
                    airspy_set_vga_gain(openDev, vgaGain);
                }
                if (selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][selectedSerStr]["vgaGain"] = vgaGain;
                    config.release(true);
                }
            }

            // AGC Control
            SmGui::ForceSync();
            if (SmGui::Checkbox(CONCAT("LNA AGC##_airspy_", name), &lnaAgc)) {
                if (running) {
                    if (lnaAgc) {
                        airspy_set_lna_agc(openDev, 1);
                    }
                    else {
                        airspy_set_lna_agc(openDev, 0);
                        airspy_set_lna_gain(openDev, lnaGain);
                    }
                }
                if (selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][selectedSerStr]["lnaAgc"] = lnaAgc;
                    config.release(true);
                }
            }
            SmGui::ForceSync();
            if (SmGui::Checkbox(CONCAT("Mixer AGC##_airspy_", name), &mixerAgc)) {
                if (running) {
                    if (mixerAgc) {
                        airspy_set_mixer_agc(openDev, 1);
                    }
                    else {
                        airspy_set_mixer_agc(openDev, 0);
                        airspy_set_mixer_gain(openDev, mixerGain);
                    }
                }
                if (selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][selectedSerStr]["mixerAgc"] = mixerAgc;
                    config.release(true);
                }
            }
        }

        // Bias T
        if (SmGui::Checkbox(CONCAT("Bias T##_airspy_", name), &biasT)) {
            if (running) {
                airspy_set_rf_bias(openDev, biasT);
            }
            if (selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][selectedSerStr]["biasT"] = biasT;
                config.release(true);
            }
        }
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

#ifdef __ANDROID__
    int devFd = 0;
#endif

    std::vector<uint64_t> devList;
    std::string devListTxt;
    std::vector<uint32_t> sampleRateList;
    std::string sampleRateListTxt;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/airspy_config.json");
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