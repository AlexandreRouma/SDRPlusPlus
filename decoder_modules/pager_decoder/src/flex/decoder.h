#pragma once
#include "../decoder.h"
#include <signal_path/vfo_manager.h>
#include <utils/optionlist.h>
#include <gui/widgets/symbol_diagram.h>
#include <gui/style.h>
#include <dsp/sink/handler_sink.h>
#include "flex.h"

class FLEXDecoder : public Decoder {
    dsp::stream<float> dummy1;
    dsp::stream<uint8_t> dummy2;
public:
    FLEXDecoder(const std::string& name, VFOManager::VFO* vfo) : diag(0.6, 1600) {
        this->name = name;
        this->vfo = vfo;

        // Define baudrate options
        baudrates.define(1600, "1600 Baud", 1600);
        baudrates.define(3200, "3200 Baud", 3200);
        baudrates.define(6400, "6400 Baud", 6400);

        // Init DSP
        vfo->setBandwidthLimits(12500, 12500, true);
        vfo->setSampleRate(16000, 12500);
        reshape.init(&dummy1, 1600.0, (1600 / 30.0) - 1600.0);
        dataHandler.init(&dummy2, _dataHandler, this);
        diagHandler.init(&reshape.out, _diagHandler, this);
    }

    ~FLEXDecoder() {
        stop();
    }

    void showMenu() {
        ImGui::LeftLabel("Baudrate");
        ImGui::FillWidth();
        if (ImGui::Combo(("##pager_decoder_flex_br_" + name).c_str(), &brId, baudrates.txt)) {
            // TODO
        }

        ImGui::FillWidth();
        diag.draw();
    }

    void setVFO(VFOManager::VFO* vfo) {
        this->vfo = vfo;
        vfo->setBandwidthLimits(12500, 12500, true);
        vfo->setSampleRate(24000, 12500);
        // dsp.setInput(vfo->output);
    }

    void start() {
        flog::debug("FLEX start");
        // dsp.start();
        reshape.start();
        dataHandler.start();
        diagHandler.start();
    }

    void stop() {
        flog::debug("FLEX stop");
        // dsp.stop();
        reshape.stop();
        dataHandler.stop();
        diagHandler.stop();
    }

private:
    static void _dataHandler(uint8_t* data, int count, void* ctx) {
        FLEXDecoder* _this = (FLEXDecoder*)ctx;
        // _this->decoder.process(data, count);
    }

    static void _diagHandler(float* data, int count, void* ctx) {
        FLEXDecoder* _this = (FLEXDecoder*)ctx;
        float* buf = _this->diag.acquireBuffer();
        memcpy(buf, data, count * sizeof(float));
        _this->diag.releaseBuffer();
    }

    std::string name;

    VFOManager::VFO* vfo;
    dsp::buffer::Reshaper<float> reshape;
    dsp::sink::Handler<uint8_t> dataHandler;
    dsp::sink::Handler<float> diagHandler;

    flex::Decoder decoder;

    ImGui::SymbolDiagram diag;

    int brId = 0;

    OptionList<int, int> baudrates;
};