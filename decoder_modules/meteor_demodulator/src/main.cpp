#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <filesystem>
#include "meteor_demod.h"
#include <dsp/routing/splitter.h>
#include <dsp/buffer/reshaper.h>
#include <dsp/sink/handler_sink.h>
#include <meteor_demodulator_interface.h>
#include <gui/widgets/folder_select.h>
#include <gui/widgets/constellation_diagram.h>

#include <fstream>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "meteor_demodulator",
    /* Description:     */ "Meteor demodulator for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

ConfigManager config;

std::string genFileName(std::string prefix, std::string suffix) {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buf[1024];
    sprintf(buf, "%s_%02d-%02d-%02d_%02d-%02d-%02d%s", prefix.c_str(), ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900, suffix.c_str());
    return buf;
}

#define INPUT_SAMPLE_RATE 150000

class MeteorDemodulatorModule : public ModuleManager::Instance {
public:
    MeteorDemodulatorModule(std::string name) : folderSelect("%ROOT%/recordings") {
        this->name = name;

        writeBuffer = new int8_t[STREAM_BUFFER_SIZE];

        // Load config
        config.acquire();
        // Note: this first one may not be needed but I'm paranoid
        if (!config.conf.contains(name)) {
            config.conf[name] = json({});
        }
        if (config.conf[name].contains("recPath")) {
            folderSelect.setPath(config.conf[name]["recPath"]);
        }
        if (config.conf[name].contains("brokenModulation")) {
            brokenModulation = config.conf[name]["brokenModulation"];
        }
        if (config.conf[name].contains("oqpsk")) {
            oqpsk = config.conf[name]["oqpsk"];
        }
        if (config.conf[name].contains("symbolrate")) {
            symbolrate = config.conf[name]["symbolrate"];
        }
        config.release();

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, INPUT_SAMPLE_RATE, true);
        demod.init(vfo->output, (double)symbolrate, INPUT_SAMPLE_RATE, 33, 0.6f, 0.1f, 0.005f, brokenModulation, oqpsk, 1e-6, 0.01);
        split.init(&demod.out);
        split.bindStream(&symSinkStream);
        split.bindStream(&sinkStream);
        reshape.init(&symSinkStream, 1024, (72000 / 30) - 1024);
        symSink.init(&reshape.out, symSinkHandler, this);
        sink.init(&sinkStream, sinkHandler, this);

        demod.start();
        split.start();
        reshape.start();
        symSink.start();
        sink.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
        core::modComManager.registerInterface("meteor_demodulator", name, moduleInterfaceHandler, this);
    }

    ~MeteorDemodulatorModule() {
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

    void postInit() {}

    void enable() {
        double bw = gui::waterfall.getBandwidth();
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, std::clamp<double>(0, -bw / 2.0, bw / 2.0), 150000, INPUT_SAMPLE_RATE, 150000, 150000, true);

        demod.setBrokenModulation(brokenModulation);
        demod.setInput(vfo->output);

        demod.start();
        split.start();
        reshape.start();
        symSink.start();
        sink.start();

        enabled = true;
    }

    void disable() {
        demod.stop();
        split.stop();
        reshape.stop();
        symSink.stop();
        sink.stop();

        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        MeteorDemodulatorModule* _this = (MeteorDemodulatorModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        _this->constDiagram.draw();

        if (_this->folderSelect.render("##meteor_rec" + _this->name)) {
            if (_this->folderSelect.pathIsValid()) {
                config.acquire();
                config.conf[_this->name]["recPath"] = _this->folderSelect.path;
                config.release(true);
            }
        }

        if (ImGui::Checkbox(CONCAT("Broken modulation##meteor_rec", _this->name), &_this->brokenModulation)) {
            _this->demod.setBrokenModulation(_this->brokenModulation);
            config.acquire();
            config.conf[_this->name]["brokenModulation"] = _this->brokenModulation;
            config.release(true);
        }

        if (ImGui::Checkbox(CONCAT("OQPSK##oqpsk", _this->name), &_this->oqpsk)) {
            _this->demod.setOQPSK(_this->oqpsk);
            config.acquire();
            config.conf[_this->name]["oqpsk"] = _this->oqpsk;
            config.release(true);
        }

        ImGui::BeginGroup();
        ImGui::Columns(2, CONCAT("Symbolrate##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("72k##_", _this->name), _this->symbolrate == 72000) && _this->symbolrate != 72000) {
            _this->symbolrate = 72000;
            _this->demod.setSymbolrate((double)_this->symbolrate);
            config.acquire();
            config.conf[_this->name]["symbolrate"] = _this->symbolrate;
            config.release(true);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("80k##_", _this->name), _this->symbolrate == 80000) && _this->symbolrate != 80000) {
            _this->symbolrate = 80000;
            _this->demod.setSymbolrate((double)_this->symbolrate);
            config.acquire();
            config.conf[_this->name]["symbolrate"] = _this->symbolrate;
            config.release(true);
        }
        ImGui::Columns(1, CONCAT("EndSymbolrate##_", _this->name), false);
        ImGui::EndGroup();

        if (!_this->folderSelect.pathIsValid() && _this->enabled) { style::beginDisabled(); }

        if (_this->recording) {
            if (ImGui::Button(CONCAT("Stop##meteor_rec_", _this->name), ImVec2(menuWidth, 0))) {
                _this->stopRecording();
            }
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %.2fMB", (float)_this->dataWritten / 1000000.0f);
        }
        else {
            if (ImGui::Button(CONCAT("Record##meteor_rec_", _this->name), ImVec2(menuWidth, 0))) {
                _this->startRecording();
            }
            ImGui::TextUnformatted("Idle --.--MB");
        }

        if (!_this->folderSelect.pathIsValid() && _this->enabled) { style::endDisabled(); }

        if (!_this->enabled) { style::endDisabled(); }
    }

    static void symSinkHandler(dsp::complex_t* data, int count, void* ctx) {
        MeteorDemodulatorModule* _this = (MeteorDemodulatorModule*)ctx;

        dsp::complex_t* buf = _this->constDiagram.acquireBuffer();
        memcpy(buf, data, 1024 * sizeof(dsp::complex_t));
        _this->constDiagram.releaseBuffer();
    }

    static void sinkHandler(dsp::complex_t* data, int count, void* ctx) {
        MeteorDemodulatorModule* _this = (MeteorDemodulatorModule*)ctx;
        std::lock_guard<std::mutex> lck(_this->recMtx);
        if (!_this->recording) { return; }
        for (int i = 0; i < count; i++) {
            _this->writeBuffer[(2 * i)] = std::clamp<int>(data[i].re * 84.0f, -127, 127);
            _this->writeBuffer[(2 * i) + 1] = std::clamp<int>(data[i].im * 84.0f, -127, 127);
        }
        _this->recFile.write((char*)_this->writeBuffer, count * 2);
        _this->dataWritten += count * 2;
    }

    void startRecording() {
        std::lock_guard<std::mutex> lck(recMtx);
        dataWritten = 0;
        std::string filename = genFileName(folderSelect.expandString(folderSelect.path) + "/meteor", ".s");
        recFile = std::ofstream(filename, std::ios::binary);
        if (recFile.is_open()) {
            flog::info("Recording to '{0}'", filename);
            recording = true;
        }
        else {
            flog::error("Could not open file for recording!");
        }
    }

    void stopRecording() {
        std::lock_guard<std::mutex> lck(recMtx);
        recording = false;
        recFile.close();
        dataWritten = 0;
    }

    static void moduleInterfaceHandler(int code, void* in, void* out, void* ctx) {
        MeteorDemodulatorModule* _this = (MeteorDemodulatorModule*)ctx;
        if (code == METEOR_DEMODULATOR_IFACE_CMD_START) {
            if (!_this->recording) { _this->startRecording(); }
        }
        else if (code == METEOR_DEMODULATOR_IFACE_CMD_STOP) {
            if (_this->recording) { _this->stopRecording(); }
        }
    }

    std::string name;
    bool enabled = true;

    // DSP Chain
    VFOManager::VFO* vfo;
    dsp::demod::Meteor demod;
    dsp::routing::Splitter<dsp::complex_t> split;

    dsp::stream<dsp::complex_t> symSinkStream;
    dsp::stream<dsp::complex_t> sinkStream;
    dsp::buffer::Reshaper<dsp::complex_t> reshape;
    dsp::sink::Handler<dsp::complex_t> symSink;
    dsp::sink::Handler<dsp::complex_t> sink;

    ImGui::ConstellationDiagram constDiagram;

    FolderSelect folderSelect;

    std::mutex recMtx;
    bool recording = false;
    uint64_t dataWritten = 0;
    std::ofstream recFile;
    bool brokenModulation = false;
    bool oqpsk = false;
    uint64_t symbolrate = 72000;
    int8_t* writeBuffer;
};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    std::string root = (std::string)core::args["root"];
    if (!std::filesystem::exists(root + "/recordings")) {
        flog::warn("Recordings directory does not exist, creating it");
        if (!std::filesystem::create_directory(root + "/recordings")) {
            flog::error("Could not create recordings directory");
        }
    }
    json def = json({});
    config.setPath(root + "/meteor_demodulator_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new MeteorDemodulatorModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (MeteorDemodulatorModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
