#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <filesystem>
#include <dsp/stream.h>
#include <dsp/buffer/reshaper.h>
#include <dsp/multirate/rational_resampler.h>
#include <dsp/sink/handler_sink.h>
#include <fstream>
#include <chrono>
#include "dab_dsp.h"
#include <gui/widgets/constellation_diagram.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "dab_decoder",
    /* Description:     */ "DAB/DAB+ Decoder for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

ConfigManager config;

#define INPUT_SAMPLE_RATE   2.048e6
#define VFO_BANDWIDTH       1.6e6

class M17DecoderModule : public ModuleManager::Instance {
public:
    M17DecoderModule(std::string name)  {
        this->name = name;

        file = std::ofstream("sync4.f32", std::ios::out | std::ios::binary);

        // Load config
        config.acquire();
        
        config.release(true);

        // Initialize VFO
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, VFO_BANDWIDTH, INPUT_SAMPLE_RATE, VFO_BANDWIDTH, VFO_BANDWIDTH, true);
        vfo->setSnapInterval(250);

        // Initialize DSP here
        csync.init(vfo->output, 1e-3, 246e-6, INPUT_SAMPLE_RATE);
        ffsync.init(&csync.out);
        ns.init(&ffsync.out, handler, this);

        // Start DSO Here
        csync.start();
        ffsync.start();
        ns.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~M17DecoderModule() {
        gui::menu.removeEntry(name);
        // Stop DSP Here
        if (enabled) {
            csync.stop();
            ffsync.stop();
            ns.stop();
            sigpath::vfoManager.deleteVFO(vfo);
        }

        sigpath::sinkManager.unregisterStream(name);
    }

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw / 2.0, bw / 2.0), VFO_BANDWIDTH, INPUT_SAMPLE_RATE, VFO_BANDWIDTH, VFO_BANDWIDTH, true);
        vfo->setSnapInterval(250);

        // Set Input of demod here
        csync.setInput(vfo->output);

        // Start DSP here
        csync.start();
        ffsync.start();
        ns.start();

        enabled = true;
    }

    void disable() {
        // Stop DSP here
        csync.stop();
        ffsync.stop();
        ns.stop();

        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        M17DecoderModule* _this = (M17DecoderModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (!_this->enabled) { style::beginDisabled(); }

        _this->constDiagram.draw();

        if (!_this->enabled) { style::endDisabled(); }
    }

    std::ofstream file;

    static void handler(dsp::complex_t* data, int count, void* ctx) {
        M17DecoderModule* _this = (M17DecoderModule*)ctx;
        //_this->file.write((char*)data, count * sizeof(dsp::complex_t));

        dsp::complex_t* buf = _this->constDiagram.acquireBuffer();
        memcpy(buf, data, 1024 * sizeof(dsp::complex_t));
        _this->constDiagram.releaseBuffer();
    }

    std::string name;
    bool enabled = true;

    dab::CyclicSync csync;
    dab::FrameFreqSync ffsync;
    dsp::sink::Handler<dsp::complex_t> ns;

    ImGui::ConstellationDiagram constDiagram;

    // DSP Chain
    VFOManager::VFO* vfo;
};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    json def = json({});
    config.setPath(core::args["root"].s() + "/dab_decoder_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new M17DecoderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (M17DecoderModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
