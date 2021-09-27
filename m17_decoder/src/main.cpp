#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <options.h>
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
#include <m17dsp.h>
#include <fstream>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "m17_decoder",
    /* Description:     */ "M17 Digital Voice Decoder for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

ConfigManager config;

#define INPUT_SAMPLE_RATE 14400

class M17DecoderModule : public ModuleManager::Instance {
public:
    M17DecoderModule(std::string name) : diag(1.0f, 480) {
        this->name = name;

        // Load config

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 9600, INPUT_SAMPLE_RATE, 9600, 9600, true);
        vfo->setSnapInterval(500);

        // Intialize DSP here
        decoder.init(vfo->output, INPUT_SAMPLE_RATE);

        resampWin.init(4000, 4000, audioSampRate);
        resamp.init(decoder.out, &resampWin, 8000, audioSampRate);
        resampWin.setSampleRate(8000 * resamp.getInterpolation());
        resamp.updateWindow(&resampWin);

        reshape.init(decoder.diagOut, 480, 0);
        diagHandler.init(&reshape.out, _diagHandler, this);

        // Start DSO Here
        decoder.start();
        resamp.start();
        reshape.start();
        diagHandler.start();

        // Setup audio stream
        srChangeHandler.ctx = this;
        srChangeHandler.handler = sampleRateChangeHandler;
        stream.init(&resamp.out, &srChangeHandler, audioSampRate);
        sigpath::sinkManager.registerStream(name, &stream);

        stream.start();
               
        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~M17DecoderModule() {
        // Stop DSP Here
        decoder.stop();
        resamp.stop();
        reshape.stop();
        diagHandler.stop();

        stream.stop();

        sigpath::vfoManager.deleteVFO(vfo);
        gui::menu.removeEntry(name);
    }

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw/2.0, bw/2.0), 9600, INPUT_SAMPLE_RATE, 9600, 9600, true);
        vfo->setSnapInterval(500);

        // Set Input of demod here
        decoder.setInput(vfo->output);

        // Start DSP here
        decoder.start();
        resamp.start();
        reshape.start();
        diagHandler.start();

        enabled = true;
    }

    void disable() {
        // Stop DSP here
        decoder.stop();
        resamp.stop();
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

        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        _this->diag.draw();

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void _diagHandler(float* data, int count, void* ctx) {
        M17DecoderModule* _this = (M17DecoderModule*)ctx;
        float* buf = _this->diag.acquireBuffer();
        memcpy(buf, data, count * sizeof(float));
        _this->diag.releaseBuffer();
    }

    static void sampleRateChangeHandler(float sampleRate, void* ctx) {
        M17DecoderModule* _this = (M17DecoderModule*)ctx;
        // TODO: If too slow, change all demods here and not when setting
        _this->audioSampRate = sampleRate;
        _this->resampWin.setCutoff(std::min<float>(sampleRate/2, 4000));
        _this->resamp.tempStop();
        _this->resamp.setOutSampleRate(sampleRate);
        _this->resampWin.setSampleRate(8000 * _this->resamp.getInterpolation());
        _this->resamp.updateWindow(&_this->resampWin);
        _this->resamp.tempStart();
    }

    std::string name;
    bool enabled = true;

    // DSP Chain
    VFOManager::VFO* vfo;

    dsp::M17Decoder decoder;
    
    dsp::Reshaper<float> reshape;
    dsp::HandlerSink<float> diagHandler;

    dsp::filter_window::BlackmanWindow resampWin;
    dsp::PolyphaseResampler<dsp::stereo_t> resamp;

    ImGui::SymbolDiagram diag;

    double audioSampRate = 48000;
    EventHandler<float> srChangeHandler;
    SinkManager::Stream stream;

};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    json def = json({});
    config.setPath(options::opts.root + "/m17_decoder_config.json");
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