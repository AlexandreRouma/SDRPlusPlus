#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <filesystem>
#include <dsp/routing/splitter.h>
#include <dsp/buffer/reshaper.h>
#include <dsp/sink/handler_sink.h>
#include <gui/widgets/folder_select.h>
#include <gui/widgets/constellation_diagram.h>
#include "ryfi/receiver.h"

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "ryfi_decoder",
    /* Description:     */ "RyFi decoder for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define INPUT_BANDWIDTH     800000
#define INPUT_SAMPLE_RATE   1500000
#define INPUT_BAUDRATE      720000

#define SYMBOL_DIAG_RATE    30
#define SYMBOL_DIAG_COUNT   1024

class RyFiDecoderModule : public ModuleManager::Instance {
public:
    RyFiDecoderModule(std::string name) {
        this->name = name;

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, INPUT_BANDWIDTH, INPUT_SAMPLE_RATE, INPUT_BANDWIDTH, INPUT_BANDWIDTH, true);
        rx.init(vfo->output, INPUT_BAUDRATE, INPUT_SAMPLE_RATE);
        reshape.init(rx.softOut, SYMBOL_DIAG_COUNT, (INPUT_BAUDRATE / SYMBOL_DIAG_RATE) - SYMBOL_DIAG_COUNT);
        symSink.init(&reshape.out, symSinkHandler, this);
        rx.onPacket.bind(&RyFiDecoderModule::packetHandler, this);

        rx.start();
        reshape.start();
        symSink.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~RyFiDecoderModule() {
        rx.stop();
        reshape.stop();
        symSink.stop();
        sigpath::vfoManager.deleteVFO(vfo);
        gui::menu.removeEntry(name);
    }

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw / 2.0, bw / 2.0), INPUT_BANDWIDTH, INPUT_SAMPLE_RATE, INPUT_BANDWIDTH, INPUT_BANDWIDTH, true);

        rx.setInput(vfo->output);

        rx.start();
        reshape.start();
        symSink.start();

        enabled = true;
    }

    void disable() {
        rx.stop();
        reshape.stop();
        symSink.stop();

        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    void packetHandler(ryfi::Packet pkt) {
        flog::debug("Got a {} byte packet!", pkt.size());
    }

    static void menuHandler(void* ctx) {
        RyFiDecoderModule* _this = (RyFiDecoderModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        _this->constDiagram.draw();

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void symSinkHandler(dsp::complex_t* data, int count, void* ctx) {
        RyFiDecoderModule* _this = (RyFiDecoderModule*)ctx;

        dsp::complex_t* buf = _this->constDiagram.acquireBuffer();
        memcpy(buf, data, 1024 * sizeof(dsp::complex_t));
        _this->constDiagram.releaseBuffer();
    }

    std::string name;
    bool enabled = true;

    // DSP Chain
    VFOManager::VFO* vfo;
    ryfi::Receiver rx;
    dsp::buffer::Reshaper<dsp::complex_t> reshape;
    dsp::sink::Handler<dsp::complex_t> symSink;

    ImGui::ConstellationDiagram constDiagram;
};

MOD_EXPORT void _INIT_() {

}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RyFiDecoderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RyFiDecoderModule*)instance;
}

MOD_EXPORT void _END_() {

}