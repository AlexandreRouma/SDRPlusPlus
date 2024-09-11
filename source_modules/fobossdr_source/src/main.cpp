#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>
#include <atomic>
#include <fobos.h>

SDRPP_MOD_INFO{
    /* Name:            */ "fobossdr_source",
    /* Description:     */ "FobosSDR Source Module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class FobosSDRSourceModule : public ModuleManager::Instance {
public:
    FobosSDRSourceModule(std::string name) {
        this->name = name;

        sampleRate = 50000000.0;

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

        sigpath::sourceManager.registerSource("FobosSDR", &handler);
    }

    ~FobosSDRSourceModule() {
        
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
        
        // Get device list
        char serials[1024];
        memset(serials, 0, sizeof(serials));
        int devCount = fobos_rx_list_devices(serials);
        if (devCount < 0) {
            flog::error("Failed to get device list: {}", devCount);
            return;
        }

        // If no device, give up
        if (!devCount) { return; }

        // Generate device entries
        const char* _serials = serials;
        int index = 0;
        while (*_serials) {
            // Read serial until space
            std::string serial = "";
            while (*_serials) {
                // Get a character
                char c = *(_serials++);

                // If it's a space, we're done
                if (c == ' ') { break; }

                // Otherwise, add it to the string
                serial += c;
            }

            // Create entry
            devices.define(serial, serial, index++);
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

        // Get the ID in the list
        int id = devices.keyId(serial);
        selectedDevId = devices[id];

        // Open the device
        fobos_dev_t* dev;
        int err = fobos_rx_open(&dev, selectedDevId);
        if (err) {
            flog::error("Failed to open device: {}", err);
            return;
        }

        // Get a list of supported samplerates
        double srList[128];
        unsigned int srCount;
        err = fobos_rx_get_samplerates(dev, srList, &srCount);
        if (err) {
            flog::error("Failed to get samplerate list: {}", err);
            return;
        }

        // Generate samplerate list
        samplerates.clear();
        for (int i = 0; i < srCount; i++) {
            std::string str = getBandwdithScaled(srList[i]);
            samplerates.define(srList[i], str, srList[i]);
        }

        // Define the ports
        ports.clear();
        ports.define("rf", "RF", PORT_RF);
        ports.define("hf1", "HF1", PORT_HF1);
        ports.define("hf2", "HF2", PORT_HF2);

        // Define clock sources
        clockSources.clear();
        clockSources.define("internal", "Internal", 0);
        clockSources.define("external", "External", 1);

        // Close the device
        fobos_rx_close(dev);

        // Update the samplerate
        core::setInputSampleRate(sampleRate);

        // Save serial number
        selectedSerial = serial;
        devId = id;
    }

    static void menuSelected(void* ctx) {
        FobosSDRSourceModule* _this = (FobosSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("FobosSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        FobosSDRSourceModule* _this = (FobosSDRSourceModule*)ctx;
        flog::info("FobosSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        FobosSDRSourceModule* _this = (FobosSDRSourceModule*)ctx;
        if (_this->running) { return; }

        // Open the device
        int err = fobos_rx_open(&_this->openDev, _this->selectedDevId);
        if (err) {
            flog::error("Failed to open device: {}", err);
            return;
        }

        // Get the selected port
        _this->port = _this->ports[_this->portId];

        // Configure the device
        double actualSr, actualFreq;
        fobos_rx_set_samplerate(_this->openDev, _this->sampleRate, &actualSr);
        fobos_rx_set_frequency(_this->openDev, _this->freq, &actualFreq);
        fobos_rx_set_direct_sampling(_this->openDev, _this->port != PORT_RF);
        fobos_rx_set_clk_source(_this->openDev, _this->clockSources[_this->clkSrcId]);
        fobos_rx_set_lna_gain(_this->openDev, _this->lnaGain);
        fobos_rx_set_vga_gain(_this->openDev, _this->vgaGain);

        // Compute buffer size
        _this->bufferSize = _this->sampleRate / 200.0;

        // Start streaming
        err = fobos_rx_start_sync(_this->openDev, _this->bufferSize);
        if (err) {
            flog::error("Failed to start stream: {}", err);
            return;
        }

        // Start worker
        _this->run = true;
        _this->workerThread = std::thread(&FobosSDRSourceModule::worker, _this);
        
        _this->running = true;
        flog::info("FobosSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        FobosSDRSourceModule* _this = (FobosSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;

        // Stop worker
        _this->run = false;
        _this->stream.stopWriter();
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();

        // Stop streaming
        fobos_rx_stop_sync(_this->openDev);

        // Close the device
        fobos_rx_close(_this->openDev);

        flog::info("FobosSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        FobosSDRSourceModule* _this = (FobosSDRSourceModule*)ctx;
        if (_this->running) {
            double actual; // Dummy, don't care
            fobos_rx_set_frequency(_this->openDev, freq, &actual);
        }
        _this->freq = freq;
        flog::info("FobosSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        FobosSDRSourceModule* _this = (FobosSDRSourceModule*)ctx;
        
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_fobossdr_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        if (SmGui::Combo(CONCAT("##_fobossdr_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_fobossdr_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        SmGui::LeftLabel("Antenna Port");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_fobossdr_port_", _this->name), &_this->portId, _this->ports.txt));

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Clock Source");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_fobossdr_clk_", _this->name), &_this->clkSrcId, _this->clockSources.txt));

        if (_this->port == PORT_RF) {
            SmGui::LeftLabel("LNA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_fobossdr_lna_gain_", _this->name), &_this->lnaGain, 0, 2)) {
                if (_this->running) {
                    fobos_rx_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                // TODO: Save
            }

            SmGui::LeftLabel("VGA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_fobossdr_vga_gain_", _this->name), &_this->vgaGain, 0, 15)) {
                if (_this->running) {
                    fobos_rx_set_vga_gain(_this->openDev, _this->vgaGain);
                }
                // TODO: Save
            }
        }
    }

    void worker() {
        // Worker loop
        while (run) {
            // Read samples
            unsigned int sampCount = 0;
            int err = fobos_rx_read_sync(openDev, (float*)stream.writeBuf, &sampCount);
            
            // TODO: Send to DSP
            if (!stream.swap(sampCount)) { break; }
        }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    OptionList<std::string, int> devices;
    OptionList<int, double> samplerates;
    OptionList<std::string, Port> ports;
    OptionList<std::string, int> clockSources;
    int devId = 0;
    int srId = 0;
    int portId = 0;
    int clkSrcId = 0;
    Port port;
    int lnaGain = 0;
    int vgaGain = 0;
    std::string selectedSerial;
    int selectedDevId;

    fobos_dev_t* openDev;

    int bufferSize;
    std::thread workerThread;
    std::atomic<bool> run = false;
};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new FobosSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (FobosSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}