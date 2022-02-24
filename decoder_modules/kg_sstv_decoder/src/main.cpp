#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <filesystem>
#include <dsp/pll.h>
#include <dsp/stream.h>
#include <dsp/demodulator.h>
#include <dsp/window.h>
#include <dsp/resampling.h>
#include <dsp/processing.h>
#include <dsp/routing.h>
#include <dsp/sink.h>
#include <gui/widgets/folder_select.h>
#include <gui/widgets/symbol_diagram.h>
#include <fstream>
#include <chrono>
#include "kg_sstv_dsp.h"

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "kg_sstv_decoder",
    /* Description:     */ "KG-SSTV Digital SSTV Decoder for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

ConfigManager config;

#define INPUT_SAMPLE_RATE 6000

class M17DecoderModule : public ModuleManager::Instance {
public:
    M17DecoderModule(std::string name) : diag(0.8, 480) {
        this->name = name;

        // Load config
        config.acquire();
        if (!config.conf.contains(name)) {
            config.conf[name]["showLines"] = false;
        }
        showLines = config.conf[name]["showLines"];
        if (showLines) {
            diag.lines.push_back(-0.75f);
            diag.lines.push_back(-0.25f);
            diag.lines.push_back(0.25f);
            diag.lines.push_back(0.75f);
        }
        config.release(true);


        // Initialize VFO
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 3000, INPUT_SAMPLE_RATE, 3000, 3000, true);
        vfo->setSnapInterval(250);

        // Initialize DSP here
        decoder.init(vfo->output, INPUT_SAMPLE_RATE);

        reshape.init(decoder.diagOut, 480, 0);
        diagHandler.init(&reshape.out, _diagHandler, this);

        // Start DSO Here
        decoder.start();
        reshape.start();
        diagHandler.start();

        //stream.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~M17DecoderModule() {
        gui::menu.removeEntry(name);
        // Stop DSP Here
        if (enabled) {
            decoder.stop();
            reshape.stop();
            diagHandler.stop();
            sigpath::vfoManager.deleteVFO(vfo);
        }

        sigpath::sinkManager.unregisterStream(name);
    }

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw / 2.0, bw / 2.0), 9600, INPUT_SAMPLE_RATE, 9600, 9600, true);
        vfo->setSnapInterval(250);

        // Set Input of demod here
        decoder.setInput(vfo->output);

        // Start DSP here
        decoder.start();
        reshape.start();
        diagHandler.start();

        enabled = true;
    }

    void disable() {
        // Stop DSP here
        decoder.stop();
        reshape.stop();
        diagHandler.stop();

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

        ImGui::SetNextItemWidth(menuWidth);
        _this->diag.draw();

        if (ImGui::Checkbox(CONCAT("Show Reference Lines##m17_showlines_", _this->name), &_this->showLines)) {
            if (_this->showLines) {
                _this->diag.lines.push_back(-0.75f);
                _this->diag.lines.push_back(-0.25f);
                _this->diag.lines.push_back(0.25f);
                _this->diag.lines.push_back(0.75f);
            }
            else {
                _this->diag.lines.clear();
            }
            config.acquire();
            config.conf[_this->name]["showLines"] = _this->showLines;
            config.release(true);
        }

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void _diagHandler(float* data, int count, void* ctx) {
        M17DecoderModule* _this = (M17DecoderModule*)ctx;
        float* buf = _this->diag.acquireBuffer();
        memcpy(buf, data, count * sizeof(float));
        _this->diag.releaseBuffer();
    }

    std::string name;
    bool enabled = true;

    // DSP Chain
    VFOManager::VFO* vfo;
    kgsstv::Decoder decoder;

    dsp::Reshaper<float> reshape;
    dsp::HandlerSink<float> diagHandler;
    dsp::stream<float> dummy;

    ImGui::SymbolDiagram diag;

    bool showLines = false;
};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    json def = json({});
    config.setPath(core::args["root"].s() + "/kg_sstv_decoder_config.json");
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
