#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>
#include <atomic>
#include <sddc.h>

SDRPP_MOD_INFO{
    /* Name:            */ "sddc_source",
    /* Description:     */ "SDDC Source Module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ -1
};

ConfigManager config;

#define CONCAT(a, b) ((std::string(a) + b).c_str())


class SDDCSourceModule : public ModuleManager::Instance {
public:
    SDDCSourceModule(std::string name) {
        this->name = name;

        // Set firmware image path for debugging
        sddc_set_firmware_path("C:/Users/ryzerth/Downloads/SDDC_FX3 (1).img");

        sampleRate = 128e6;

        // Initialize the DDC
        ddc.init(&ddcIn, 50e6, 50e6, 50e6, 0.0);

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

        // Select device from config
        config.acquire();
        std::string devSerial = config.conf["device"];
        config.release();
        select(devSerial);

        sigpath::sourceManager.registerSource("SDDC", &handler);
    }

    ~SDDCSourceModule() {
        // Nothing to do
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

    enum Port {
        PORT_RF,
        PORT_HF1,
        PORT_HF2
    };

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

    void refresh() {
        devices.clear();
        
        // // Get device list
        // sddc_devinfo_t* devList;
        // int count = sddc_get_device_list(&devList);
        // if (count < 0) {
        //     flog::error("Failed to list SDDC devices: {}", count);
        //     return;
        // }

        // // Add every device found
        // for (int i = 0; i < count; i++) {
        //     // Create device name
        //     std::string name = sddc_model_to_string(devList[i].model);
        //     name += '[';
        //     name += devList[i].serial;
        //     name += ']';

        //     // Add an entry to the device list
        //     devices.define(devList[i].serial, name, devList[i].serial);
        // }

        devices.define("0009072C00C40C32", "TESTING", "0009072C00C40C32");

        // // Free the device list
        // sddc_free_device_list(devList);
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

        // Get the ID in the list
        int id = devices.keyId(serial);

        // Open the device
        sddc_dev_t* dev;
        int err = sddc_open(serial.c_str(), &dev);
        if (err) {
            flog::error("Failed to open device: {}", err);
            return;
        }

        // Generate samplerate list
        samplerates.clear();
        samplerates.define(4e6, "4 MHz", 4e6);
        samplerates.define(8e6, "8 MHz", 8e6);
        samplerates.define(16e6, "16 MHz", 16e6);
        samplerates.define(32e6, "32 MHz", 32e6);
        samplerates.define(64e6, "64 MHz", 64e6);

        // // Define the ports
        // ports.clear();
        // ports.define("hf", "HF", PORT_RF);
        // ports.define("vhf", "VHF", PORT_HF1);

        // Close the device
        sddc_close(dev);

        // Save serial number
        selectedSerial = serial;
        devId = id;

        // Load default options
        sampleRate = 64e6;
        srId = samplerates.valueId(sampleRate);
        // port = PORT_RF;
        // portId = ports.valueId(port);
        // lnaGain = 0;
        // vgaGain = 0;

        // Load config
        config.acquire();
        if (config.conf["devices"][selectedSerial].contains("samplerate")) {
            int desiredSr = config.conf["devices"][selectedSerial]["samplerate"];
            if (samplerates.keyExists(desiredSr)) {
                srId = samplerates.keyId(desiredSr);
                sampleRate = samplerates[srId];
            }
        }
        // if (config.conf["devices"][selectedSerial].contains("port")) {
        //     std::string desiredPort = config.conf["devices"][selectedSerial]["port"];
        //     if (ports.keyExists(desiredPort)) {
        //         portId = ports.keyId(desiredPort);
        //         port = ports[portId];
        //     }
        // }
        // if (config.conf["devices"][selectedSerial].contains("lnaGain")) {
        //     lnaGain = std::clamp<int>(config.conf["devices"][selectedSerial]["lnaGain"], FOBOS_LNA_GAIN_MIN, FOBOS_LNA_GAIN_MAX);
        // }
        // if (config.conf["devices"][selectedSerial].contains("vgaGain")) {
        //     vgaGain = std::clamp<int>(config.conf["devices"][selectedSerial]["vgaGain"], FOBOS_VGA_GAIN_MIN, FOBOS_VGA_GAIN_MAX);
        // }
        config.release();

        // Update the samplerate
        core::setInputSampleRate(sampleRate);
    }

    static void menuSelected(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("SDDCSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        flog::info("SDDCSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (_this->running) { return; }

        // Open the device
        sddc_error_t err = sddc_open(_this->selectedSerial.c_str(), &_this->openDev);
        if (err) {
            flog::error("Failed to open device: {}", (int)err);
            return;
        }

        // // Get the selected port
        // _this->port = _this->ports[_this->portId];

        // Configure the device
        sddc_set_samplerate(_this->openDev, _this->sampleRate * 2);

        // // Configure the DDC
        // if (_this->port == PORT_RF && _this->sampleRate >= 50e6) {
        //     // Set the frequency
        //     fobos_rx_set_frequency(_this->openDev, _this->freq, &actualFreq);
        // }
        // else if (_this->port == PORT_RF) {
        //     // Set the frequency
        //     fobos_rx_set_frequency(_this->openDev, _this->freq, &actualFreq);

        //     // Configure and start the DDC for decimation only
        //     _this->ddc.setInSamplerate(actualSr);
        //     _this->ddc.setOutSamplerate(_this->sampleRate, _this->sampleRate);
        //     _this->ddc.setOffset(0.0);
        //     _this->ddc.start();
        // }
        // else {
            // Configure and start the DDC
            _this->ddc.setInSamplerate(_this->sampleRate * 2);
            _this->ddc.setOutSamplerate(_this->sampleRate, _this->sampleRate);
            _this->ddc.setOffset(_this->freq);
            _this->ddc.start();
        // }

        // Compute buffer size (Lower than usual, but it's a workaround for their API having broken streaming)
        _this->bufferSize = _this->sampleRate / 100.0;

        // Start streaming
        err = sddc_start(_this->openDev);
        if (err) {
            flog::error("Failed to start stream: {}", (int)err);
            return;
        }

        // Start worker
        _this->run = true;
        _this->workerThread = std::thread(&SDDCSourceModule::worker, _this);
        
        _this->running = true;
        flog::info("SDDCSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;

        // Stop worker
        _this->run = false;
        if (_this->port == PORT_RF && _this->sampleRate >= 50e6) {
            _this->ddc.out.stopWriter();
            if (_this->workerThread.joinable()) { _this->workerThread.join(); }
            _this->ddc.out.clearWriteStop();
        }
        else {
            _this->ddcIn.stopWriter();
            if (_this->workerThread.joinable()) { _this->workerThread.join(); }
            _this->ddcIn.clearWriteStop();
        }

        // Stop streaming
        sddc_stop(_this->openDev);

        // Stop the DDC
        _this->ddc.stop();

        // Close the device
        sddc_close(_this->openDev);

        flog::info("SDDCSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (_this->running) {
            // if (_this->port == PORT_RF) {
            //     double actual; // Dummy, don't care
            //     //fobos_rx_set_frequency(_this->openDev, freq, &actual);
            // }
            // else {
                _this->ddc.setOffset(freq);
            // }
        }
        _this->freq = freq;
        flog::info("SDDCSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_sddc_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["device"] = _this->selectedSerial;
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_sddc_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["samplerate"] = _this->samplerates.key(_this->srId);
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_sddc_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        // SmGui::LeftLabel("Antenna Port");
        // SmGui::FillWidth();
        // if (SmGui::Combo(CONCAT("##_sddc_port_", _this->name), &_this->portId, _this->ports.txt)) {
        //     if (!_this->selectedSerial.empty()) {
        //         config.acquire();
        //         config.conf["devices"][_this->selectedSerial]["port"] = _this->ports.key(_this->portId);
        //         config.release(true);
        //     }
        // }

        if (_this->running) { SmGui::EndDisabled(); }

        // if (_this->port == PORT_RF) {
        //     SmGui::LeftLabel("LNA Gain");
        //     SmGui::FillWidth();
        //     if (SmGui::SliderInt(CONCAT("##_sddc_lna_gain_", _this->name), &_this->lnaGain, FOBOS_LNA_GAIN_MIN, FOBOS_LNA_GAIN_MAX)) {
        //         if (_this->running) {
        //             fobos_rx_set_lna_gain(_this->openDev, _this->lnaGain);
        //         }
        //         if (!_this->selectedSerial.empty()) {
        //             config.acquire();
        //             config.conf["devices"][_this->selectedSerial]["lnaGain"] = _this->lnaGain;
        //             config.release(true);
        //         }
        //     }

        //     SmGui::LeftLabel("VGA Gain");
        //     SmGui::FillWidth();
        //     if (SmGui::SliderInt(CONCAT("##_sddc_vga_gain_", _this->name), &_this->vgaGain, FOBOS_VGA_GAIN_MIN, FOBOS_VGA_GAIN_MAX)) {
        //         if (_this->running) {
        //             fobos_rx_set_vga_gain(_this->openDev, _this->vgaGain);
        //         }
        //         if (!_this->selectedSerial.empty()) {
        //             config.acquire();
        //             config.conf["devices"][_this->selectedSerial]["vgaGain"] = _this->vgaGain;
        //             config.release(true);
        //         }
        //     }
        // }
    }

    void worker() {
        // // Select different processing depending on the mode
        // if (port == PORT_RF && sampleRate >= 50e6) {
        //     while (run) {
        //         // Read samples
        //         unsigned int sampCount = 0;
        //         int err = fobos_rx_read_sync(openDev, (float*)ddc.out.writeBuf, &sampCount);
        //         if (err) { break; }
                
        //         // Send out samples to the core
        //         if (!ddc.out.swap(sampCount)) { break; }
        //     }
        // }
        // else if (port == PORT_RF) {
        //     while (run) {
        //         // Read samples
        //         unsigned int sampCount = 0;
        //         int err = fobos_rx_read_sync(openDev, (float*)ddcIn.writeBuf, &sampCount);
        //         if (err) { break; }
                
        //         // Send samples to the DDC
        //         if (!ddcIn.swap(sampCount)) { break; }
        //     }
        // }
        // else if (port == PORT_HF1) {
        //     while (run) {
        //         // Read samples
        //         unsigned int sampCount = 0;
        //         int err = fobos_rx_read_sync(openDev, (float*)ddcIn.writeBuf, &sampCount);
        //         if (err) { break; }

        //         // Null out the HF2 samples
        //         for (int i = 0; i < sampCount; i++) {
        //             ddcIn.writeBuf[i].im = 0.0f;
        //         }
                
        //         // Send samples to the DDC
        //         if (!ddcIn.swap(sampCount)) { break; }
        //     }
        // }
        // else if (port == PORT_HF2) {
            // Allocate the sample buffer
            int16_t* buffer = dsp::buffer::alloc<int16_t>(bufferSize);
            float* fbuffer = dsp::buffer::alloc<float>(bufferSize);
            float* nullBuffer = dsp::buffer::alloc<float>(bufferSize);

            // Clear the null buffer
            dsp::buffer::clear(nullBuffer, bufferSize);

            while (run) {
                // Read samples
                int err = sddc_rx(openDev, buffer, bufferSize);
                if (err) { break; }

                // Convert the samples to float
                volk_16i_s32f_convert_32f(fbuffer, buffer, 32768.0f, bufferSize);

                // Interleave into a complex value
                volk_32f_x2_interleave_32fc((lv_32fc_t*)ddcIn.writeBuf, fbuffer, nullBuffer, bufferSize);
                
                // Send samples to the DDC
                if (!ddcIn.swap(bufferSize)) { break; }
            }

            // Free the buffer
            dsp::buffer::free(buffer);
        // }
    }

    std::string name;
    bool enabled = true;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    OptionList<std::string, std::string> devices;
    OptionList<int, int> samplerates;
    OptionList<std::string, Port> ports;
    int devId = 0;
    int srId = 0;
    int portId = 0;
    Port port;
    int lnaGain = 0;
    int vgaGain = 0;
    std::string selectedSerial;

    sddc_dev_t* openDev;

    int bufferSize;
    std::thread workerThread;
    std::atomic<bool> run = false;

    dsp::stream<dsp::complex_t> ddcIn;
    dsp::channel::RxVFO ddc;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/sddc_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SDDCSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (SDDCSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}