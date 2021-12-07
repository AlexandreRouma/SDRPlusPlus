#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <options.h>
#include <filesystem>
#include <dsp/stream.h>
#include <dsp/demodulator.h>
#include <dsp/window.h>
#include <dsp/routing.h>
#include <dsp/sink.h>
#include <goes_demodulator_interface.h>
#include <gui/widgets/folder_select.h>
#include <gui/widgets/constellation_diagram.h>

#include <fstream>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
        /* Name:            */ "goes_demodulator",
        /* Description:     */ "GOES 16/17 demodulator for SDR++",
        /* Author:          */ "Lucas Teske / Ryzerth",
        /* Version:         */ 0, 1, 0,
        /* Max instances    */ -1
};

ConfigManager config;

std::string genFileName(const std::string &prefix, const std::string &suffix) {
    time_t now = time(nullptr);
    tm *ltm = localtime(&now);
    char buf[1024];
    sprintf(buf, "%s_%02d-%02d-%02d_%02d-%02d-%02d%s", prefix.c_str(), ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900, suffix.c_str());
    return buf;
}

#define INPUT_SAMPLE_RATE 2000000
#define BAUD_RATE 927000
#define BANDWIDTH (BAUD_RATE*2)
#define RRC 0.3

class GoesDemodulatorModule : public ModuleManager::Instance {
public:
    explicit GoesDemodulatorModule(std::string name) : folderSelect("%ROOT%/recordings") {
        this->name = name;

        writeBuffer = new int8_t[STREAM_BUFFER_SIZE];

        // Load config
        config.acquire();
        bool created = false;
        if (!config.conf.contains(name)) {
            config.conf[name]["recPath"] = "%ROOT%/recordings";
            created = true;
        }
        folderSelect.setPath(config.conf[name]["recPath"]);
        config.release(created);

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, BANDWIDTH, INPUT_SAMPLE_RATE, BANDWIDTH, BANDWIDTH, true);
        demod.init(vfo->output, INPUT_SAMPLE_RATE, BAUD_RATE, 63, RRC, 0.01f, 0.001f);
        split.init(demod.out);
        split.bindStream(&symSinkStream);
        split.bindStream(&sinkStream);
        reshape.init(&symSinkStream, 1024, (BAUD_RATE / 30) - 1024);
        symSink.init(&reshape.out, symSinkHandler, this);
        sink.init(&sinkStream, sinkHandler, this);

        demod.start();
        split.start();
        reshape.start();
        symSink.start();
        sink.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
        core::modComManager.registerInterface("goes_demodulator", name, moduleInterfaceHandler, this);
    }

    ~GoesDemodulatorModule() {
        if (recording) {
            std::lock_guard<std::mutex> lck(recMtx);
            recording = false;
            recFile.close();
        }
        demod.stop();
        split.stop();
        reshape.stop();
        symSink.stop();
        sink.stop();
        sigpath::vfoManager.deleteVFO(vfo);
        gui::menu.removeEntry(name);
    }

    void postInit() override {}

    void enable() override {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw / 2.0, bw / 2.0), BANDWIDTH, INPUT_SAMPLE_RATE, BANDWIDTH, BANDWIDTH, true);

        demod.setInput(vfo->output);

        demod.start();
        split.start();
        reshape.start();
        symSink.start();
        sink.start();

        enabled = true;
    }

    void disable() override {
        demod.stop();
        split.stop();
        reshape.stop();
        symSink.stop();
        sink.stop();

        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() override {
        return enabled;
    }

private:
    static void menuHandler(void *ctx) {
        auto _this = (GoesDemodulatorModule *) ctx;

        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        _this->constDiagram.draw();

        if (_this->folderSelect.render("##Goes-recorder-" + _this->name)) {
            if (_this->folderSelect.pathIsValid()) {
                config.acquire();
                config.conf[_this->name]["recPath"] = _this->folderSelect.path;
                config.release(true);
            }
        }

        if (!_this->folderSelect.pathIsValid() && _this->enabled) { style::beginDisabled(); }

        if (_this->recording) {
            if (ImGui::Button(CONCAT("Stop##_recorder_rec_", _this->name), ImVec2(menuWidth, 0))) {
                _this->stopRecording();
            }
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %.2fMB", (float) _this->dataWritten / 1000000.0f);
        } else {
            if (ImGui::Button(CONCAT("Record##_recorder_rec_", _this->name), ImVec2(menuWidth, 0))) {
                _this->startRecording();
            }
            ImGui::Text("Idle --.--MB");
        }

        if (!_this->folderSelect.pathIsValid() && _this->enabled) { style::endDisabled(); }

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void symSinkHandler(dsp::complex_t *data, int count, void *ctx) {
        auto _this = (GoesDemodulatorModule *) ctx;

        dsp::complex_t *buf = _this->constDiagram.acquireBuffer();
        memcpy(buf, data, 1024 * sizeof(dsp::complex_t));
        _this->constDiagram.releaseBuffer();
    }

    static void sinkHandler(dsp::complex_t *data, int count, void *ctx) {
        auto _this = (GoesDemodulatorModule *) ctx;
        std::lock_guard<std::mutex> lck(_this->recMtx);
        if (!_this->recording) { return; }
        for (int i = 0; i < count; i++) {
            _this->writeBuffer[i] = (int8_t) std::clamp<int>((int) (data[i].im * 84.0f), -127, 127);
        }
        _this->recFile.write((char *) _this->writeBuffer, count);
        _this->dataWritten += count;
    }

    void startRecording() {
        std::lock_guard<std::mutex> lck(recMtx);
        dataWritten = 0;
        std::string filename = genFileName(folderSelect.expandString(folderSelect.path) + "/Goes", ".s");
        recFile = std::ofstream(filename, std::ios::binary);
        if (recFile.is_open()) {
            spdlog::info("Recording to '{0}'", filename);
            recording = true;
        } else {
            spdlog::error("Could not open file for recording!");
        }
    }

    void stopRecording() {
        std::lock_guard<std::mutex> lck(recMtx);
        recording = false;
        recFile.close();
        dataWritten = 0;
    }

    static void moduleInterfaceHandler(int code, void *in, void *out, void *ctx) {
        auto _this = (GoesDemodulatorModule *) ctx;
        if (code == GOES_DEMODULATOR_IFACE_CMD_START) {
            if (!_this->recording) { _this->startRecording(); }
        } else if (code == GOES_DEMODULATOR_IFACE_CMD_STOP) {
            if (_this->recording) { _this->stopRecording(); }
        }
    }

    std::string name;
    bool enabled = true;

    // DSP Chain
    VFOManager::VFO *vfo;
    dsp::PSKDemod<2, false> demod;
    dsp::Splitter<dsp::complex_t> split;

    dsp::stream<dsp::complex_t> symSinkStream;
    dsp::stream<dsp::complex_t> sinkStream;
    dsp::Reshaper<dsp::complex_t> reshape;
    dsp::HandlerSink<dsp::complex_t> symSink;
    dsp::HandlerSink<dsp::complex_t> sink;

    ImGui::ConstellationDiagram constDiagram;

    FolderSelect folderSelect;

    std::mutex recMtx;
    bool recording = false;
    uint64_t dataWritten = 0;
    std::ofstream recFile;

    int8_t *writeBuffer;

};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    if (!std::filesystem::exists(options::opts.root + "/recordings")) {
        spdlog::warn("Recordings directory does not exist, creating it");
        if (!std::filesystem::create_directory(options::opts.root + "/recordings")) {
            spdlog::error("Could not create recordings directory");
        }
    }
    json def = json({});
    config.setPath(options::opts.root + "/Goes_demodulator_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance *_CREATE_INSTANCE_(std::string name) {
    return new GoesDemodulatorModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void *instance) {
    delete (GoesDemodulatorModule *) instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}