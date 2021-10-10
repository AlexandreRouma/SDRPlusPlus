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
#include <chrono>

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
    M17DecoderModule(std::string name) : diag(0.8, 480) {
        this->name = name;
        lsf.valid = false;

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
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 9600, INPUT_SAMPLE_RATE, 9600, 9600, true);
        vfo->setSnapInterval(250);

        // Initialize DSP here
        decoder.init(vfo->output, INPUT_SAMPLE_RATE, lsfHandler, this);

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
        gui::menu.removeEntry(name);
        // Stop DSP Here
        stream.stop();
        if (enabled) {
            decoder.stop();
            resamp.stop();
            reshape.stop();
            diagHandler.stop();
            sigpath::vfoManager.deleteVFO(vfo);
        }

        sigpath::sinkManager.unregisterStream(name);
    }

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw/2.0, bw/2.0), 9600, INPUT_SAMPLE_RATE, 9600, 9600, true);
        vfo->setSnapInterval(250);

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

        {
            std::lock_guard lck(_this->lsfMtx);

            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now-_this->lastUpdated).count() > 1000) {
                _this->lsf.valid = false;
            }

            ImGui::BeginTable(CONCAT("##m17_info_tbl_", _this->name), 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders);
            if (!_this->lsf.valid) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Source");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("--");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Destination");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("--");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Data Type");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("--");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Encryption");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("-- (Subtype --)");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("CAN");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("--");
            }
            else {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Source");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(_this->lsf.src.c_str());

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Destination");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(_this->lsf.dst.c_str());

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Data Type");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(M17DataTypesTxt[_this->lsf.dataType]);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Encryption");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s (Subtype %d)", M17EncryptionTypesTxt[_this->lsf.encryptionType], _this->lsf.encryptionSubType);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("CAN");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", _this->lsf.channelAccessNum);
            }
            ImGui::EndTable();
        }

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

    static void lsfHandler(M17LSF& lsf, void* ctx) {
        M17DecoderModule* _this = (M17DecoderModule*)ctx;
        std::lock_guard lck(_this->lsfMtx);
        _this->lastUpdated = std::chrono::high_resolution_clock::now();
        _this->lsf = lsf;
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

    bool showLines = false;

    M17LSF lsf;
    std::mutex lsfMtx;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastUpdated;
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
