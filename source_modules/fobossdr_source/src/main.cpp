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

ConfigManager config;

#define CONCAT(a, b) ((std::string(a) + b).c_str())

// Work around for the fobos API not including
#define FOBOS_LNA_GAIN_MIN  1
#define FOBOS_LNA_GAIN_MAX  3
#define FOBOS_VGA_GAIN_MIN  0
#define FOBOS_VGA_GAIN_MAX  31

class FobosSDRSourceModule : public ModuleManager::Instance {
public:
    FobosSDRSourceModule(std::string name) {
        this->name = name;

        sampleRate = 50000000.0;

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

        sigpath::sourceManager.registerSource("FobosSDR", &handler);
    }

    ~FobosSDRSourceModule() {
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

        // Add some custom samplerates
        samplerates.define(5e6, "5.0MHz", 5e6);
        samplerates.define(2.5e6, "2.5MHz", 2.5e6);
        samplerates.define(1.25e6, "1.25MHz", 1.25e6);

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

        // Save serial number
        selectedSerial = serial;
        devId = id;

        // Load default options
        sampleRate = 50e6;
        srId = samplerates.valueId(sampleRate);
        port = PORT_RF;
        portId = ports.valueId(port);
        clkSrcId = clockSources.nameId("Internal");
        lnaGain = 0;
        vgaGain = 0;

        // Load config
        config.acquire();
        if (config.conf["devices"][selectedSerial].contains("samplerate")) {
            int desiredSr = config.conf["devices"][selectedSerial]["samplerate"];
            if (samplerates.keyExists(desiredSr)) {
                srId = samplerates.keyId(desiredSr);
                sampleRate = samplerates[srId];
            }
        }
        if (config.conf["devices"][selectedSerial].contains("port")) {
            std::string desiredPort = config.conf["devices"][selectedSerial]["port"];
            if (ports.keyExists(desiredPort)) {
                portId = ports.keyId(desiredPort);
                port = ports[portId];
            }
        }
        if (config.conf["devices"][selectedSerial].contains("clkSrc")) {
            std::string desiredClkSrc = config.conf["devices"][selectedSerial]["clkSrc"];
            if (clockSources.keyExists(desiredClkSrc)) {
                clkSrcId = clockSources.keyId(desiredClkSrc);
            }
        }
        if (config.conf["devices"][selectedSerial].contains("lnaGain")) {
            lnaGain = std::clamp<int>(config.conf["devices"][selectedSerial]["lnaGain"], FOBOS_LNA_GAIN_MIN, FOBOS_LNA_GAIN_MAX);
        }
        if (config.conf["devices"][selectedSerial].contains("vgaGain")) {
            vgaGain = std::clamp<int>(config.conf["devices"][selectedSerial]["vgaGain"], FOBOS_VGA_GAIN_MIN, FOBOS_VGA_GAIN_MAX);
        }
        config.release();

        // Update the samplerate
        core::setInputSampleRate(sampleRate);
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
        fobos_rx_set_samplerate(_this->openDev, (_this->sampleRate >= 50e6) ? _this->sampleRate : 50e6, &actualSr);
        fobos_rx_set_frequency(_this->openDev, _this->freq, &actualFreq);
        fobos_rx_set_direct_sampling(_this->openDev, _this->port != PORT_RF);
        fobos_rx_set_clk_source(_this->openDev, _this->clockSources[_this->clkSrcId]);
        fobos_rx_set_lna_gain(_this->openDev, _this->lnaGain);
        fobos_rx_set_vga_gain(_this->openDev, _this->vgaGain);

        // Configure the DDC
        if (_this->port == PORT_RF && _this->sampleRate >= 50e6) {
            // Set the frequency
            fobos_rx_set_frequency(_this->openDev, _this->freq, &actualFreq);
        }
        else if (_this->port == PORT_RF) {
            // Set the frequency
            fobos_rx_set_frequency(_this->openDev, _this->freq, &actualFreq);

            // Configure and start the DDC for decimation only
            _this->ddc.setInSamplerate(actualSr);
            _this->ddc.setOutSamplerate(_this->sampleRate, _this->sampleRate);
            _this->ddc.setOffset(0.0);
            _this->ddc.start();
        }
        else {
            // Configure and start the DDC
            _this->ddc.setInSamplerate(actualSr);
            _this->ddc.setOutSamplerate(_this->sampleRate, _this->sampleRate);
            _this->ddc.setOffset(_this->freq);
            _this->ddc.start();
        }

        // Compute buffer size (Lower than usual, but it's a workaround for their API having broken streaming)
        _this->bufferSize = _this->sampleRate / 400.0;

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
        fobos_rx_stop_sync(_this->openDev);

        // Stop the DDC
        _this->ddc.stop();

        // Close the device
        fobos_rx_close(_this->openDev);

        flog::info("FobosSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        FobosSDRSourceModule* _this = (FobosSDRSourceModule*)ctx;
        if (_this->running) {
            if (_this->port == PORT_RF) {
                double actual; // Dummy, don't care
                fobos_rx_set_frequency(_this->openDev, freq, &actual);
            }
            else {
                _this->ddc.setOffset(freq);
            }
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
            config.acquire();
            config.conf["device"] = _this->selectedSerial;
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_fobossdr_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
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
        if (SmGui::Button(CONCAT("Refresh##_fobossdr_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        SmGui::LeftLabel("Antenna Port");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_fobossdr_port_", _this->name), &_this->portId, _this->ports.txt)) {
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["port"] = _this->ports.key(_this->portId);
                config.release(true);
            }
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Clock Source");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_fobossdr_clk_", _this->name), &_this->clkSrcId, _this->clockSources.txt)) {
            if (_this->running) {
                fobos_rx_set_clk_source(_this->openDev, _this->clockSources[_this->clkSrcId]);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["clkSrc"] = _this->clockSources.key(_this->clkSrcId);
                config.release(true);
            }
        }

        if (_this->port == PORT_RF) {
            SmGui::LeftLabel("LNA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_fobossdr_lna_gain_", _this->name), &_this->lnaGain, FOBOS_LNA_GAIN_MIN, FOBOS_LNA_GAIN_MAX)) {
                if (_this->running) {
                    fobos_rx_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                if (!_this->selectedSerial.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerial]["lnaGain"] = _this->lnaGain;
                    config.release(true);
                }
            }

            SmGui::LeftLabel("VGA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_fobossdr_vga_gain_", _this->name), &_this->vgaGain, FOBOS_VGA_GAIN_MIN, FOBOS_VGA_GAIN_MAX)) {
                if (_this->running) {
                    fobos_rx_set_vga_gain(_this->openDev, _this->vgaGain);
                }
                if (!_this->selectedSerial.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerial]["vgaGain"] = _this->vgaGain;
                    config.release(true);
                }
            }
        }
    }

    void worker() {
        // Select different processing depending on the mode
        if (port == PORT_RF && sampleRate >= 50e6) {
            while (run) {
                // Read samples
                unsigned int sampCount = 0;
                int err = fobos_rx_read_sync(openDev, (float*)ddc.out.writeBuf, &sampCount);
                if (err) { break; }
                
                // Send out samples to the core
                if (!ddc.out.swap(sampCount)) { break; }
            }
        }
        else if (port == PORT_RF) {
            while (run) {
                // Read samples
                unsigned int sampCount = 0;
                int err = fobos_rx_read_sync(openDev, (float*)ddcIn.writeBuf, &sampCount);
                if (err) { break; }
                
                // Send samples to the DDC
                if (!ddcIn.swap(sampCount)) { break; }
            }
        }
        else if (port == PORT_HF1) {
            while (run) {
                // Read samples
                unsigned int sampCount = 0;
                int err = fobos_rx_read_sync(openDev, (float*)ddcIn.writeBuf, &sampCount);
                if (err) { break; }

                // Null out the HF2 samples
                for (int i = 0; i < sampCount; i++) {
                    ddcIn.writeBuf[i].im = 0.0f;
                }
                
                // Send samples to the DDC
                if (!ddcIn.swap(sampCount)) { break; }
            }
        }
        else if (port == PORT_HF2) {
            while (run) {
                // Read samples
                unsigned int sampCount = 0;
                int err = fobos_rx_read_sync(openDev, (float*)ddcIn.writeBuf, &sampCount);
                if (err) { break; }

                // Null out the HF2 samples
                for (int i = 0; i < sampCount; i++) {
                    ddcIn.writeBuf[i].re = 0.0f;
                }
                
                // Send samples to the DDC
                if (!ddcIn.swap(sampCount)) { break; }
            }
        }
    }

    std::string name;
    bool enabled = true;
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

    dsp::stream<dsp::complex_t> ddcIn;
    dsp::channel::RxVFO ddc;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/fobossdr_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new FobosSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (FobosSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}