#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <options.h>
#include <gui/gui.h>
#include <dsp/pll.h>
#include <dsp/stream.h>
#include <dsp/demodulator.h>
#include <dsp/window.h>
#include <dsp/resampling.h>
#include <dsp/processing.h>
#include <dsp/routing.h>

#include <dsp/deframing.h>
#include <falcon_fec.h>
#include <falcon_packet.h>
#include <dsp/sink.h>

#include <gui/widgets/symbol_diagram.h>

#include <fstream>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "falcon9_decoder",
    /* Description:     */ "Falcon9 telemetry decoder for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define INPUT_SAMPLE_RATE   6000000

std::ofstream file("output.ts");

class Falcon9DecoderModule : public ModuleManager::Instance {
public:
    Falcon9DecoderModule(std::string name) {
        this->name = name;

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 4000000, INPUT_SAMPLE_RATE, 4000000, 4000000, true);

        // dsp::Splitter<float> split;
        // dsp::Reshaper<float> reshape;
        // dsp::HandlerSink<float> symSink;
        // dsp::stream<float> thrInput;
        // dsp::Threshold thr;

        demod.init(vfo->output, INPUT_SAMPLE_RATE, 2000000.0f);
        recov.init(&demod.out, (float)INPUT_SAMPLE_RATE / 3571400.0f, powf(0.01f, 2) / 4.0f, 0.01, 100e-6f); // 0.00765625f, 0.175f, 0.005f
        split.init(&recov.out);
        split.bindStream(&reshapeInput);
        split.bindStream(&thrInput);
        reshape.init(&reshapeInput, 1024, 198976);
        symSink.init(&reshape.out, symSinkHandler, this);
        thr.init(&thrInput);
        deframe.init(&thr.out, 10232, syncWord, 32);
        falconRS.init(&deframe.out);
        pkt.init(&falconRS.out);
        sink.init(&pkt.out, sinkHandler, this);

        demod.start();
        recov.start();
        split.start();
        reshape.start();
        symSink.start();
        sink.start();
        pkt.start();
        falconRS.start();
        deframe.start();
        thr.start();

#ifdef _WIN32
        ffplay = _popen("ffplay -framedrop -infbuf -hide_banner -loglevel panic -window_title \"Falcon 9 Cameras\" -", "wb");
#else
        ffplay = popen("ffplay -framedrop -infbuf -hide_banner -loglevel panic -window_title \"Falcon 9 Cameras\" -", "wb");
#endif

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~Falcon9DecoderModule() {
        
    }

    void postInit() {}

    void enable() {
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 4000000, INPUT_SAMPLE_RATE, 4000000, 4000000, true);

        demod.setInput(vfo->output);

        demod.start();
        recov.start();
        split.start();
        reshape.start();
        symSink.start();
        sink.start();
        pkt.start();
        falconRS.start();
        deframe.start();
        thr.start();

        enabled = true;
    }

    void disable() {
        demod.stop();
        recov.stop();
        split.stop();
        reshape.stop();
        symSink.stop();
        sink.stop();
        pkt.stop();
        falconRS.stop();
        deframe.stop();
        thr.stop();

        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        Falcon9DecoderModule* _this = (Falcon9DecoderModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        _this->symDiag.draw();

        if (_this->logsVisible) {
            if (ImGui::Button("Hide logs", ImVec2(menuWidth, 0))) { _this->logsVisible = false; }
        }
        else {
            if (ImGui::Button("Show logs", ImVec2(menuWidth, 0))) { _this->logsVisible = true; }
        }

        if (_this->logsVisible) {
            std::lock_guard<std::mutex> lck(_this->logsMtx);

            ImGui::Begin("Falcon9 Telemetry");
            ImGui::BeginTabBar("Falcon9Tabs");

            // GPS Logs
            ImGui::BeginTabItem("GPS");
            if (ImGui::Button("Clear logs##GPSClear")) { _this->gpsLogs.clear(); }
            ImGui::BeginChild(ImGuiID("GPSChild"));
            ImGui::TextUnformatted(_this->gpsLogs.c_str());
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();

            // STMM1A Logs
            ImGui::BeginTabItem("STMM1A");

            ImGui::EndTabItem();

            // STMM1B Logs
            ImGui::BeginTabItem("STMM1B");

            ImGui::EndTabItem();

            // STMM1C Logs
            ImGui::BeginTabItem("STMM1C");

            ImGui::EndTabItem();

            ImGui::EndTabBar();
            ImGui::End();
        }

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void sinkHandler(uint8_t* data, int count, void* ctx) {
        Falcon9DecoderModule* _this = (Falcon9DecoderModule*)ctx;

        uint16_t length = (((data[0] & 0b1111) << 8) | data[1]) + 2;
        uint64_t pktId =    ((uint64_t)data[2] << 56) | ((uint64_t)data[3] << 48) | ((uint64_t)data[4] << 40) | ((uint64_t)data[5] << 32)
                        |   ((uint64_t)data[6] << 24) | ((uint64_t)data[7] << 16) | ((uint64_t)data[8] << 8) | data[9];

        if (pktId == 0x0117FE0800320303 || pktId == 0x0112FA0800320303) { 
            data[length - 2] = 0;
            _this->logsMtx.lock();
            _this->gpsLogs += (char*)(data + 25);
            _this->logsMtx.unlock();
        }
        else if (pktId == 0x01123201042E1403) {
            fwrite(data + 25, 1, 940, _this->ffplay);
            file.write((char*)(data + 25), 940);
        }

        //printf("%016" PRIX64 ": %d bytes, %d full\n", pktId, length, count);
    }

    static void symSinkHandler(float* data, int count, void* ctx) {
        Falcon9DecoderModule* _this = (Falcon9DecoderModule*)ctx;
        float* buf = _this->symDiag.acquireBuffer();
        memcpy(buf, data, 1024*sizeof(float));
        _this->symDiag.releaseBuffer();
    }

    std::string name;
    bool enabled = true;
    
    bool logsVisible = false;

    std::mutex logsMtx;
    std::string gpsLogs = "";

    // DSP Chain
    dsp::FloatFMDemod demod;
    dsp::MMClockRecovery<float> recov;

    dsp::Splitter<float> split;

    dsp::stream<float> reshapeInput;
    dsp::Reshaper<float> reshape;
    dsp::HandlerSink<float> symSink;

    dsp::stream<float> thrInput;
    dsp::Threshold thr;

    uint8_t syncWord[32] = {0,0,0,1,1,0,1,0,1,1,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,0,1};
    dsp::Deframer deframe;
    dsp::FalconRS falconRS;
    dsp::FalconPacketSync pkt;
    dsp::HandlerSink<uint8_t> sink;

    FILE* ffplay;

    VFOManager::VFO* vfo;

    ImGui::SymbolDiagram symDiag;

};

MOD_EXPORT void _INIT_() {
    // Nothing
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new Falcon9DecoderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (Falcon9DecoderModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing either
}