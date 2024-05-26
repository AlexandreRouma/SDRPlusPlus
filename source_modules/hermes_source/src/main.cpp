#include "hermes.h"
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <gui/widgets/stepped_slider.h>
#include <dsp/routing/stream_link.h>
#include <utils/optionlist.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "hermes_source",
    /* Description:     */ "Hermes Lite 2 source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 1,
    /* Max instances    */ 1
};

ConfigManager config;

class HermesSourceModule : public ModuleManager::Instance {
public:
    HermesSourceModule(std::string name) {
        this->name = name;

        // Define samplerates
        samplerates.define(48000, "48KHz", hermes::HL_SAMP_RATE_48KHZ);
        samplerates.define(96000, "96KHz", hermes::HL_SAMP_RATE_96KHZ);
        samplerates.define(192000, "192KHz", hermes::HL_SAMP_RATE_192KHZ);
        samplerates.define(384000, "384KHz", hermes::HL_SAMP_RATE_384KHZ);

        srId = samplerates.keyId(384000);

        lnk.init(NULL, &stream);

        sampleRate = 384000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        sigpath::sourceManager.registerSource("Hermes", &handler);
    }

    ~HermesSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Hermes");
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

    // TODO: Implement select functions

private:
    void refresh() {
        char mac[128];
        char buf[128];
        devices.clear();
        auto devList = hermes::discover();
        for (auto& d : devList) {
            sprintf(mac, "%02X%02X%02X%02X%02X%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
            sprintf(buf, "Hermes-Lite 2 [%s]", mac);
            devices.define(mac, buf, d);
        }
    }

    void selectMac(std::string mac) {
        // If the device list is empty, don't select anything
        if (!devices.size()) {
            selectedMac.clear();
            return;
        }

        // If the mac doesn't exist, select the first available one instead
        if (!devices.keyExists(mac)) {
            selectMac(devices.key(0));
            return;
        }

        // Default config
        srId = samplerates.valueId(hermes::HL_SAMP_RATE_384KHZ);
        gain = 0;

        // Load config
        devId = devices.keyId(mac);
        selectedMac = mac;
        config.acquire();
        if (config.conf["devices"][selectedMac].contains("samplerate")) {
            int sr = config.conf["devices"][selectedMac]["samplerate"];
            if (samplerates.keyExists(sr)) { srId = samplerates.keyId(sr); }
        }
        if (config.conf["devices"][selectedMac].contains("gain")) {
            gain = config.conf["devices"][selectedMac]["gain"];
        }
        config.release();

        // Update host samplerate
        sampleRate = samplerates.key(srId);
    }

    static void menuSelected(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;

        if (_this->firstSelect) {
            _this->firstSelect = false;

            // Refresh
            _this->refresh();

            // Select device
            config.acquire();
            _this->selectedMac = config.conf["device"];
            config.release();
            _this->selectMac(_this->selectedMac);
        }

        core::setInputSampleRate(_this->sampleRate);
        flog::info("HermesSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        flog::info("HermesSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        if (_this->running || _this->selectedMac.empty()) { return; }
        
        // TODO: Implement start
        _this->dev = hermes::open(_this->devices[_this->devId].addr);

        // TODO: STOP USING A LINK, FIND A BETTER WAY
        _this->lnk.setInput(&_this->dev->out);
        _this->lnk.start();
        _this->dev->start();

        // TODO: Check if the USB commands are accepted before start
        _this->dev->setSamplerate(_this->samplerates[_this->srId]);
        _this->dev->setFrequency(_this->freq);
        _this->dev->setGain(_this->gain);

        _this->running = true;
        flog::info("HermesSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // TODO: Implement stop
        _this->dev->stop();
        _this->dev->close();
        _this->lnk.stop();

        flog::info("HermesSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        if (_this->running) {
            // TODO: Check if dev exists
            _this->dev->setFrequency(freq);
        }
        _this->freq = freq;
        flog::info("HermesSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_hermes_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->selectMac(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedMac.empty()) {
                config.acquire();
                config.conf["device"] = _this->devices.key(_this->devId);
                config.release(true);
            }
        }

        if (SmGui::Combo(CONCAT("##_hermes_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.key(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedMac.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedMac]["samplerate"] = _this->samplerates.key(_this->srId);
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_hermes_refr_", _this->name))) {
            _this->refresh();
            config.acquire();
            std::string mac = config.conf["device"];
            config.release();
            _this->selectMac(mac);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        // TODO: Device parameters

        SmGui::LeftLabel("LNA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt("##hermes_source_lna_gain", &_this->gain, 0, 60)) {
            if (_this->running) {
                _this->dev->setGain(_this->gain);
            }
            if (!_this->selectedMac.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedMac]["gain"] = _this->gain;
                config.release(true);
            }
        }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    dsp::routing::StreamLink<dsp::complex_t> lnk;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    std::string selectedMac = "";

    OptionList<std::string, hermes::Info> devices;
    OptionList<int, hermes::HermesLiteSamplerate> samplerates;

    double freq;
    int devId = 0;
    int srId = 0;
    int gain = 0;

    bool firstSelect = true;

    std::shared_ptr<hermes::Client> dev;

};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/hermes_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new HermesSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (HermesSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}