#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>
#include <dsp/channel/rx_vfo.h>
#include <dsp/correction/dc_blocker.h>
#include "badgesdr.h"

SDRPP_MOD_INFO{
    /* Name:            */ "badgesdr_source",
    /* Description:     */ "BadgeSDR Source Module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class BadgeSDRSourceModule : public ModuleManager::Instance {
public:
    BadgeSDRSourceModule(std::string name) {
        this->name = name;

        sampleRate = 250000.0;

        // Initialize DSP
        dcBlock.init(&input, 0.001);
        ddc.init(&dcBlock.out, 500000, 250000, 250000, 125000);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &ddc.out;

        // Refresh devices
        refresh();

        // Select first (TODO: Select from config)
        select("");

        sigpath::sourceManager.registerSource("BadgeSDR", &handler);
    }

    ~BadgeSDRSourceModule() {
        
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

private:
    void refresh() {
        devices.clear();
        auto list = BadgeSDR::list();
        for (const auto& info : list) {
            // Format device name
            std::string devName = "BadgeSDR ";
            devName += " [";
            devName += info.serialNumber;
            devName += ']';

            // Save device
            devices.define(info.serialNumber, devName, info);
        }
    }

    void select(const std::string& serial) {
        // If there are no devices, give up
        if (devices.empty()) {
            selectedSerial.clear();
            return;
        }

        // If the serial was not found, select the first available serial
        if (!devices.keyExists(serial)) {
            select(devices.key(0));
            return;
        }

        // Save serial number
        selectedSerial = serial;
        selectedDev = devices.value(devices.keyId(serial));
    }

    static void menuSelected(void* ctx) {
        BadgeSDRSourceModule* _this = (BadgeSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("BadgeSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        BadgeSDRSourceModule* _this = (BadgeSDRSourceModule*)ctx;
        flog::info("BadgeSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        BadgeSDRSourceModule* _this = (BadgeSDRSourceModule*)ctx;
        if (_this->running) { return; }

        // Open the device
        _this->openDev = BadgeSDR::open(_this->selectedDev);

        // Configure the device
        _this->openDev->setFrequency(_this->freq);
        _this->openDev->setLNAGain(_this->lnaGain);
        _this->openDev->setMixerGain(_this->mixerGain);
        _this->openDev->setVGAGain(_this->vgaGain);

        // Start DSP
        _this->dcBlock.start();
        _this->ddc.start();

        // Start device
        _this->openDev->start(callback, _this, 500000/200);
        
        _this->running = true;
        flog::info("BadgeSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        BadgeSDRSourceModule* _this = (BadgeSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // Stop worker
        _this->openDev->stop();

        // Stop DSP
        _this->dcBlock.stop();
        _this->ddc.stop();

        // Close device
        _this->openDev.reset();

        flog::info("BadgeSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        BadgeSDRSourceModule* _this = (BadgeSDRSourceModule*)ctx;
        if (_this->running) {
            _this->openDev->setFrequency(freq);
        }
        _this->freq = freq;
        flog::info("BadgeSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        BadgeSDRSourceModule* _this = (BadgeSDRSourceModule*)ctx;
        
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_badgesdr_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_badgesdr_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("LNA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_badgesdr_lna_gain_", _this->name), &_this->lnaGain, 0, 15)) {
            if (_this->running) {
                _this->openDev->setLNAGain(_this->lnaGain);
            }
            // TODO: Save
        }

        SmGui::LeftLabel("Mixer Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_badgesdr_mixer_gain_", _this->name), &_this->mixerGain, 0, 15)) {
            if (_this->running) {
                _this->openDev->setMixerGain(_this->mixerGain);
            }
            // TODO: Save
        }

        SmGui::LeftLabel("VGA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_badgesdr_vga_gain_", _this->name), &_this->vgaGain, 0, 15)) {
            if (_this->running) {
                _this->openDev->setVGAGain(_this->vgaGain);
            }
            // TODO: Save
        }
    }

    static void callback(const uint8_t* samples, int count, void* ctx) {
        BadgeSDRSourceModule* _this = (BadgeSDRSourceModule*)ctx;

        // Convert samples to float
        dsp::complex_t* out = _this->input.writeBuf;
        int min = 255, max = 0;
        for (int i = 0; i < count; i++) {
            if (samples[i] < min) { min = samples[i]; }
            if (samples[i] > max) { max = samples[i]; }

            out[i].re = ((float)samples[i] - 127.5f) * (1.0f/127.0f);
            out[i].im = 1.0f;
        }

        // Send out samples
        _this->input.swap(count);

        flog::debug("Amplitudes: {} -> {}", min, max);
    }

    std::string name;
    bool enabled = true;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    OptionList<std::string, BadgeSDR::DeviceInfo> devices;

    int devId = 0;
    int lnaGain = 0;
    int mixerGain = 0;
    int vgaGain = 0;
    std::string selectedSerial;
    BadgeSDR::DeviceInfo selectedDev;
    std::shared_ptr<BadgeSDR::Device> openDev;

    dsp::stream<dsp::complex_t> input;
    dsp::correction::DCBlocker<dsp::complex_t> dcBlock;
    dsp::channel::RxVFO ddc;
};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new BadgeSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (BadgeSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}