#include "hermes.h"
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <gui/widgets/stepped_slider.h>
#include <dsp/routing/stream_link.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "hermes_source",
    /* Description:     */ "Hermes Lite 2 source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class HermesSourceModule : public ModuleManager::Instance {
public:
    HermesSourceModule(std::string name) {
        this->name = name;

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
        
        refresh();

        // TODO: Select device

        sigpath::sourceManager.registerSource("Hermes", &handler);
    }

    ~HermesSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Hermes");
    }

    void postInit() {}

    enum AGCMode {
        AGC_MODE_OFF,
        AGC_MODE_LOW,
        AGC_MODE_HIGG
    };

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

        // TOOD: Update dev list
    }

    // TODO: Implement select functions

private:
    static void menuSelected(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("HermesSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        spdlog::info("HermesSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        if (_this->running) { return; }
        
        // TODO: Implement start
        _this->dev = hermes::open("192.168.0.144", 1024);

        // TODO: STOP USING A LINK, FIND A BETTER WAY
        _this->lnk.setInput(&_this->dev->out);
        _this->lnk.start();
        _this->dev->start();

        // TODO: Check if the USB commands are accepted before start
        _this->dev->setFrequency(_this->freq);
        _this->dev->setGain(_this->gain);
        _this->dev->writeReg(0, 3 << 24);

        _this->running = true;
        spdlog::info("HermesSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // TODO: Implement stop
        _this->dev->stop();
        _this->dev->close();
        _this->lnk.stop();

        spdlog::info("HermesSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;
        if (_this->running) {
            // TODO: Check if dev exists
            _this->dev->setFrequency(_this->freq);
        }
        _this->freq = freq;
        spdlog::info("HermesSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        HermesSourceModule* _this = (HermesSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        // TODO: Device selection
        if (SmGui::Button("Discover")) {
            auto disc = hermes::discover();
            spdlog::warn("Found {0} devices!", disc.size());
        }

        if (_this->running) { SmGui::EndDisabled(); }

        // TODO: Device parameters

        if (SmGui::SliderInt("Gain##hermes_source", &_this->gain, 0, 60)) {
            _this->dev->setGain(_this->gain);
        }

        if (SmGui::Button("Hermes Test")) {
            _this->dev->readReg(hermes::HL_REG_RX1_NCO_FREQ);
        }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    dsp::routing::StreamLink<dsp::complex_t> lnk;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    int gain = 0;

    std::shared_ptr<hermes::Client> dev;

    std::vector<uint64_t> devList;
    std::string devListTxt;
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