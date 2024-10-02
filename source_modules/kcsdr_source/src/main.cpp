#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>
#include "kcsdr.h"
#include <atomic>

SDRPP_MOD_INFO{
    /* Name:            */ "kcsdr_source",
    /* Description:     */ "KCSDR Source Module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class KCSDRSourceModule : public ModuleManager::Instance {
public:
    KCSDRSourceModule(std::string name) {
        this->name = name;

        sampleRate = 2000000.0;
        samplerates.define(40e6, "40MHz", 40e6);
        samplerates.define(35e6, "35MHz", 35e6);
        samplerates.define(30e6, "30MHz", 30e6);
        samplerates.define(25e6, "25MHz", 25e6);
        samplerates.define(20e6, "20MHz", 20e6);
        samplerates.define(15e6, "15MHz", 15e6);
        samplerates.define(10e6, "10MHz", 10e6);
        samplerates.define(5e6, "5MHz", 5e6);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        // Refresh devices
        refresh();

        // Select first (TODO: Select from config)
        select("");

        sigpath::sourceManager.registerSource("KCSDR", &handler);
    }

    ~KCSDRSourceModule() {
        
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
        
        // Get device list
        kcsdr_info_t* list;
        int count = kcsdr_list_devices(&list);
        if (count < 0) {
            flog::error("Failed to list devices: {}", count);
            return;
        }

        // Create list
        for (int i = 0; i < count; i++) {
            devices.define(list[i].serial, list[i].serial, list[i].serial);
        }

        // Free the device list
        kcsdr_free_device_list(list);
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

        // Get the menu ID
        devId = devices.keyId(serial);

        // TODO

        // Update the samplerate
        core::setInputSampleRate(sampleRate);

        // Save serial number
        selectedSerial = serial;
    }

    static void menuSelected(void* ctx) {
        KCSDRSourceModule* _this = (KCSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("KCSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        KCSDRSourceModule* _this = (KCSDRSourceModule*)ctx;
        flog::info("KCSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        KCSDRSourceModule* _this = (KCSDRSourceModule*)ctx;
        if (_this->running) { return; }

        // If no serial is given, do nothing
        if (_this->selectedSerial.empty()) { return; }

        // Open the device
        int err = kcsdr_open(&_this->openDev, _this->selectedSerial.c_str());
        if (err) {
            flog::error("Failed to open device: {}", err);
            return;
        }

        // Configure the device
        kcsdr_set_port(_this->openDev, KCSDR_DIR_RX, 0);
        kcsdr_set_frequency(_this->openDev, KCSDR_DIR_RX, _this->freq);
        kcsdr_set_attenuation(_this->openDev, KCSDR_DIR_RX, _this->att);
        kcsdr_set_amp_gain(_this->openDev, KCSDR_DIR_RX, _this->gain);
        kcsdr_set_rx_ext_amp_gain(_this->openDev, _this->extGain);
        kcsdr_set_samplerate(_this->openDev, KCSDR_DIR_RX, _this->sampleRate);

        // Start the stream
        kcsdr_start(_this->openDev, KCSDR_DIR_RX);

        // Start worker
        _this->run = true;
        _this->workerThread = std::thread(&KCSDRSourceModule::worker, _this);
        
        _this->running = true;
        flog::info("KCSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        KCSDRSourceModule* _this = (KCSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // Stop worker
        _this->run = false;
        _this->stream.stopWriter();
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();

        // Stop streaming
        kcsdr_stop(_this->openDev, KCSDR_DIR_RX);

        // Close the device
        kcsdr_close(_this->openDev);

        flog::info("KCSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        KCSDRSourceModule* _this = (KCSDRSourceModule*)ctx;
        if (_this->running) {
            kcsdr_set_frequency(_this->openDev, KCSDR_DIR_RX, freq);
        }
        _this->freq = freq;
        flog::info("KCSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        KCSDRSourceModule* _this = (KCSDRSourceModule*)ctx;
        
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_kcsdr_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        if (SmGui::Combo(CONCAT("##_kcsdr_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_kcsdr_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        // SmGui::LeftLabel("RX Port");
        // SmGui::FillWidth();
        // if (SmGui::Combo(CONCAT("##_kcsdr_port_", _this->name), &_this->portId, _this->rxPorts.txt)) {
        //     if (_this->running) {
        //         // TODO
        //     }
        //     // TODO: Save
        // }

        SmGui::LeftLabel("Attenuation");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_kcsdr_att_", _this->name), &_this->att, 0, 31)) {
            if (_this->running) {
                kcsdr_set_attenuation(_this->openDev, KCSDR_DIR_RX, _this->att);
            }
            // TODO: Save
        }

        SmGui::LeftLabel("Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_kcsdr_gain_", _this->name), &_this->gain, 0, 31)) {
            if (_this->running) {
                kcsdr_set_amp_gain(_this->openDev, KCSDR_DIR_RX, _this->gain);
            }
            // TODO: Save
        }

        SmGui::LeftLabel("External Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_kcsdr_ext_gain_", _this->name), &_this->extGain, 0, 31)) {
            if (_this->running) {
                kcsdr_set_rx_ext_amp_gain(_this->openDev, _this->extGain);
            }
            // TODO: Save
        }
    }

    void worker() {
        // Compute the buffer size
        int bufferSize = 0x4000/4;//sampleRate / 200;

        // Allocate the sample buffer
        int16_t* samps = dsp::buffer::alloc<int16_t>(bufferSize*2);

        // Loop
        while (run) {
            // Read samples
            int count = kcsdr_rx(openDev, samps, bufferSize);
            if (!count) { continue; }
            if (count < 0) {
                flog::debug("Failed to read samples: {}", count);
                break;
            }

            // Convert the samples to float
            volk_16i_s32f_convert_32f((float*)stream.writeBuf, samps, 8192.0f, count*2);

            // Send out the samples
            if (!stream.swap(count)) { break; }
        }

        // Free the sample buffer
        dsp::buffer::free(samps);
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    OptionList<std::string, std::string> devices;
    OptionList<int, double> samplerates;
    int devId = 0;
    int srId = 0;
    int att = 0;
    int gain = 30;
    int extGain = 1;
    int portId = 0;
    std::string selectedSerial;

    kcsdr_t* openDev;

    std::thread workerThread;
    std::atomic<bool> run = false;
};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new KCSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (KCSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}