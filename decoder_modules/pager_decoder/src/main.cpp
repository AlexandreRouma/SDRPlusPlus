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
#include <gui/widgets/folder_select.h>
#include <gui/widgets/symbol_diagram.h>
#include <fstream>
#include <chrono>
#include <dsp/demod/quadrature.h>
#include <dsp/clock_recovery/mm.h>
#include <dsp/taps/root_raised_cosine.h>
#include <dsp/correction/dc_blocker.h>
#include <dsp/loop/fast_agc.h>
#include <utils/optionlist.h>
#include <dsp/digital/binary_slicer.h>
#include <dsp/routing/doubler.h>
#include "pocsag/pocsag.h"

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "pager_decoder",
    /* Description:     */ "POCSAG and Flex Pager Decoder"
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

const char* msgTypes[] = {
    "Numeric",
    "Unknown (0b01)",
    "Unknown (0b10)",
    "Alphanumeric",
};

ConfigManager config;

#define INPUT_SAMPLE_RATE   24000.0
#define INPUT_BANDWIDTH     12500.0
#define INPUT_BAUD_RATE     2400.0

enum Protocol {
    PROTOCOL_POCSAG,
    PROTOCOL_FLEX
};

class PagerDecoderModule : public ModuleManager::Instance {
public:
    PagerDecoderModule(std::string name) : diag(0.6, 2400) {
        this->name = name;

        // Define protocols
        protocols.define("POCSAG", PROTOCOL_POCSAG);
        protocols.define("FLEX", PROTOCOL_FLEX);

        // Load config
        config.acquire();
        if (!config.conf.contains(name)) {
            config.conf[name]["showLines"] = false;
        }
        showLines = config.conf[name]["showLines"];
        if (showLines) {
            diag.lines.push_back(-1.0);
            diag.lines.push_back(-1.0/3.0);
            diag.lines.push_back(1.0/3.0);
            diag.lines.push_back(1.0);
        }
        config.release(true);

        // Initialize VFO
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, INPUT_BANDWIDTH, INPUT_SAMPLE_RATE, INPUT_BANDWIDTH, INPUT_BANDWIDTH, true);
        vfo->setSnapInterval(1);

        // Initialize DSP here (negative dev to invert signal)
        demod.init(vfo->output, -4500.0, INPUT_SAMPLE_RATE);
        dcBlock.init(&demod.out, 0.001);
        float taps[] = { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f };
        shape = dsp::taps::fromArray<float>(10, taps);
        fir.init(&dcBlock.out, shape);
        recov.init(&fir.out, INPUT_SAMPLE_RATE/INPUT_BAUD_RATE, 1e5, 0.1, 0.05);
        doubler.init(&recov.out);
        slicer.init(&doubler.outB);
        dataHandler.init(&slicer.out, _dataHandler, this);
        reshape.init(&doubler.outA, 2400.0, (INPUT_BAUD_RATE / 30.0) - 2400.0);
        diagHandler.init(&reshape.out, _diagHandler, this);

        // Initialize decode
        decoder.onMessage.bind(&PagerDecoderModule::messageHandler, this);

        // Start DSP Here
        demod.start();
        dcBlock.start();
        fir.start();
        recov.start();
        doubler.start();
        slicer.start();
        dataHandler.start();
        reshape.start();
        diagHandler.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~PagerDecoderModule() {
        gui::menu.removeEntry(name);
        // Stop DSP
        if (enabled) {
            demod.stop();
            dcBlock.stop();
            fir.stop();
            recov.stop();
            doubler.stop();
            slicer.stop();
            dataHandler.stop();
            reshape.stop();
            diagHandler.stop();
            sigpath::vfoManager.deleteVFO(vfo);
        }

        sigpath::sinkManager.unregisterStream(name);
    }

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw / 2.0, bw / 2.0), INPUT_BANDWIDTH, INPUT_SAMPLE_RATE, INPUT_BANDWIDTH, INPUT_BANDWIDTH, true);
        vfo->setSnapInterval(250);

        // Start DSP
        demod.start();
        dcBlock.start();
        fir.start();
        recov.start();
        doubler.start();
        slicer.start();
        dataHandler.start();
        reshape.start();
        diagHandler.start();

        enabled = true;
    }

    void disable() {
        demod.stop();
        dcBlock.stop();
        fir.stop();
        recov.stop();
        doubler.stop();
        slicer.stop();
        dataHandler.stop();
        reshape.stop();
        diagHandler.stop();
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
        PagerDecoderModule* _this = (PagerDecoderModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::LeftLabel("Protocol");
        ImGui::FillWidth();
        if (ImGui::Combo(("##pager_decoder_proto_" + _this->name).c_str(), &_this->protoId, _this->protocols.txt)) {
            // TODO
        }

        ImGui::SetNextItemWidth(menuWidth);
        _this->diag.draw();

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void _dataHandler(uint8_t* data, int count, void* ctx) {
        PagerDecoderModule* _this = (PagerDecoderModule*)ctx;
        _this->decoder.process(data, count);
    }

    static void _diagHandler(float* data, int count, void* ctx) {
        PagerDecoderModule* _this = (PagerDecoderModule*)ctx;
        float* buf = _this->diag.acquireBuffer();
        memcpy(buf, data, count * sizeof(float));
        _this->diag.releaseBuffer();
    }

    void messageHandler(pocsag::Address addr, pocsag::MessageType type, const std::string& msg) {
        flog::debug("[{}]: '{}'", (uint32_t)addr, msg);
    }

    std::string name;
    bool enabled = true;

    int protoId = 0;

    OptionList<std::string, Protocol> protocols;

    pocsag::Decoder decoder;

    // DSP Chain
    VFOManager::VFO* vfo;
    dsp::demod::Quadrature demod;
    dsp::correction::DCBlocker<float> dcBlock;
    dsp::tap<float> shape;
    dsp::filter::FIR<float, float> fir;
    dsp::clock_recovery::MM<float> recov;
    dsp::routing::Doubler<float> doubler;
    dsp::digital::BinarySlicer slicer;
    dsp::buffer::Reshaper<float> reshape;
    dsp::sink::Handler<uint8_t> dataHandler;
    dsp::sink::Handler<float> diagHandler;

    ImGui::SymbolDiagram diag;

    bool showLines = false;
};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    json def = json({});
    config.setPath(core::args["root"].s() + "/pager_decoder_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new PagerDecoderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (PagerDecoderModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
