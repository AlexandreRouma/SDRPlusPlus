#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <filesystem>
#include <dsp/buffer/reshaper.h>
#include <dsp/sink/handler_sink.h>
#include <gui/widgets/constellation_diagram.h>
#include "vor_decoder.h"
#include <fstream>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "vor_receiver",
    /* Description:     */ "VOR Receiver for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

ConfigManager config;

#define INPUT_SAMPLE_RATE VOR_IN_SR

class VORReceiverModule : public ModuleManager::Instance {
public:
    VORReceiverModule(std::string name) {
        this->name = name;

        // Load config
        config.acquire();
        // TODO: Load config
        config.release();

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, true);
        decoder = new vor::Decoder(vfo->output, 1);
        decoder->onBearing.bind(&VORReceiverModule::onBearing, this);

        decoder->start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~VORReceiverModule() {
        decoder->stop();
        sigpath::vfoManager.deleteVFO(vfo);
        gui::menu.removeEntry(name);
        delete decoder;
    }

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, true);

        decoder->setInput(vfo->output);

        decoder->start();

        enabled = true;
    }

    void disable() {
        decoder->stop();

        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        VORReceiverModule* _this = (VORReceiverModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::Text("Bearing: %f°", _this->bearing);
        ImGui::Text("Quality: %0.1f%%", _this->quality);

        if (!_this->enabled) { style::endDisabled(); }
    }

    void onBearing(float nbearing, float nquality) {
        bearing = (180.0f * nbearing / FL_M_PI);
        quality = nquality * 100.0f;
    }

    std::string name;
    bool enabled = true;

    // DSP Chain
    VFOManager::VFO* vfo;
    vor::Decoder* decoder;

    float bearing = 0.0f, quality = 0.0f;
};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    std::string root = (std::string)core::args["root"];
    json def = json({});
    config.setPath(root + "/vor_receiver_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new VORReceiverModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (VORReceiverModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}