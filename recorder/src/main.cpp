#include <imgui.h>
#include <module.h>
#include <wav.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <dsp/measure.h>
#include <thread>
#include <ctime>
#include <gui/gui.h>
#include <filesystem>
#include <signal_path/signal_path.h>
#include <config.h>
#include <gui/style.h>
#include <gui/widgets/volume_meter.h>
#include <regex>
#include <options.h>
#include <gui/widgets/folder_select.h>
#include <recorder_interface.h>
#include <core.h>
#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "recorder",
    /* Description:     */ "Recorder module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ -1
};

ConfigManager config;

std::string expandString(std::string input) {
    input = std::regex_replace(input, std::regex("%ROOT%"), options::opts.root);
    return std::regex_replace(input, std::regex("//"), "/");
}

std::string genFileName(std::string prefix, bool isVfo, std::string name = "") {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buf[1024];
    double freq = gui::waterfall.getCenterFrequency();;
    if (isVfo) {
        freq += gui::waterfall.vfos[name]->generalOffset;
    }
    sprintf(buf, "%.0lfHz_%02d-%02d-%02d_%02d-%02d-%02d.wav", freq, ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900);
    return prefix + buf;
}

class RecorderModule : public ModuleManager::Instance {
public:
    RecorderModule(std::string name) : folderSelect("%ROOT%/recordings") {
        this->name = name;        

        // Load config
        config.acquire();
        bool created = false;
        
        // Create config if it doesn't exist
        if (!config.conf.contains(name)) {
            config.conf[name]["mode"] = RECORDER_MODE_AUDIO;
            config.conf[name]["recPath"] = "%ROOT%/recordings";
            config.conf[name]["audioStream"] = "Radio";
            created = true;
        }

        recMode = config.conf[name]["mode"];
        folderSelect.setPath(config.conf[name]["recPath"]);
        selectedStreamName = config.conf[name]["audioStream"];
        config.release(created);

        // Init audio path
        vol.init(&dummyStream, 1.0f);
        audioSplit.init(&vol.out);
        audioSplit.bindStream(&meterStream);
        meter.init(&meterStream);
        audioHandler.init(&audioHandlerStream, _audioHandler, this);

        vol.start();
        audioSplit.start();
        meter.start();

        // Init baseband path
        basebandHandler.init(&basebandStream, _basebandHandler, this);

        wavSampleBuf = new int16_t[2 * STREAM_BUFFER_SIZE];

        refreshStreams();

        gui::menu.registerEntry(name, menuHandler, this);
        core::modComManager.registerInterface("recorder", name, moduleInterfaceHandler, this);
    }

    ~RecorderModule() {
        core::modComManager.unregisterInterface(name);
        
        // Stop recording
        if (recording) {
            if (recMode == RECORDER_MODE_AUDIO) {
                audioSplit.unbindStream(&audioHandlerStream);
                audioHandler.stop();
                audioWriter->close();
                delete audioWriter;
            }
            else {
                sigpath::signalPath.unbindIQStream(&basebandStream);
                basebandHandler.stop();
                basebandWriter->close();
                delete basebandWriter;
            }
        }

        vol.stop();
        audioSplit.stop();
        meter.stop();

        gui::menu.removeEntry(name);

        delete[] wavSampleBuf;
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    void refreshStreams() {
        std::vector<std::string> names = sigpath::sinkManager.getStreamNames();

        // If there are no stream, cancel
        if (names.size() == 0) { return; }

        // List streams
        streamNames.clear();
        streamNamesTxt = "";
        for (auto const& name : names) {
            streamNames.push_back(name);
            streamNamesTxt += name;
            streamNamesTxt += '\0';
        }

        if (selectedStreamName == "") {
            selectStream(streamNames[0]);
        }
        else {
            selectStream(selectedStreamName);
        }
    }

    void selectStream(std::string name) {
        auto it = std::find(streamNames.begin(), streamNames.end(), name);
        if (it == streamNames.end()) { return; }
        streamId = std::distance(streamNames.begin(), it);

        vol.stop();
        if (audioInput != NULL) { sigpath::sinkManager.unbindStream(selectedStreamName, audioInput); }
        audioInput = sigpath::sinkManager.bindStream(name);
        if (audioInput == NULL) { return; }
        selectedStreamName = name;
        vol.setInput(audioInput);
        vol.start();
    }

    static void menuHandler(void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

        // Recording mode
        if (_this->recording) { style::beginDisabled(); }
        ImGui::BeginGroup();
        ImGui::Columns(2, CONCAT("AirspyGainModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("Baseband##_recmode_", _this->name), _this->recMode == RECORDER_MODE_BASEBAND)) {
            _this->recMode = RECORDER_MODE_BASEBAND;
            config.acquire();
            config.conf[_this->name]["mode"] = _this->recMode;
            config.release(true);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("Audio##_recmode_", _this->name), _this->recMode == RECORDER_MODE_AUDIO)) {
            _this->recMode = RECORDER_MODE_AUDIO;
            config.acquire();
            config.conf[_this->name]["mode"] = _this->recMode;
            config.release(true);
        }
        ImGui::Columns(1, CONCAT("EndAirspyGainModeColumns##_", _this->name), false);
        ImGui::EndGroup();
        if (_this->recording) { style::endDisabled(); }

        // Recording path
        if (_this->folderSelect.render("##_recorder_fold_" + _this->name)) {
            if (_this->folderSelect.pathIsValid()) {
                config.acquire();
                config.conf[_this->name]["recPath"] = _this->folderSelect.path;
                config.release(true);
            }
        }

        // Mode specific menu
        if (_this->recMode == RECORDER_MODE_AUDIO) {
            _this->audioMenu(menuColumnWidth);
        }
        else {
            _this->basebandMenu(menuColumnWidth);
        }
    }

    void basebandMenu(float menuColumnWidth) {
        if (!folderSelect.pathIsValid()) { style::beginDisabled(); }
        if (!recording) {
            if (ImGui::Button(CONCAT("Record##_recorder_rec_", name), ImVec2(menuColumnWidth, 0))) {
                std::lock_guard lck(recMtx);
                startRecording();
            }
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
        }
        else {
            if (ImGui::Button(CONCAT("Stop##_recorder_rec_", name), ImVec2(menuColumnWidth, 0))) {
                std::lock_guard lck(recMtx);
                stopRecording();
            }
            uint64_t seconds = samplesWritten / (uint64_t)sampleRate;
            time_t diff = seconds;
            tm *dtm = gmtime(&diff);
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
        }
        if (!folderSelect.pathIsValid()) { style::endDisabled(); }
    }

    void audioMenu(float menuColumnWidth) {
        ImGui::PushItemWidth(menuColumnWidth);

        if (streamNames.size() == 0) {
            refreshStreams();
            ImGui::PopItemWidth();
            return;
        }

        if (recording) { style::beginDisabled(); }
        if (ImGui::Combo(CONCAT("##_recorder_strm_", name), &streamId, streamNamesTxt.c_str())) {
            selectStream(streamNames[streamId]);
            config.acquire();
            config.conf[name]["audioStream"] = streamNames[streamId];
            config.release(true);
        }
        if (recording) { style::endDisabled(); }

        double frameTime = 1.0 / ImGui::GetIO().Framerate;
        lvlL = std::max<float>(lvlL - (frameTime * 50.0), -90);
        lvlR = std::max<float>(lvlR - (frameTime * 50.0), -90);

        float _lvlL = meter.getLeftLevel();
        float _lvlR = meter.getRightLevel();
        if (_lvlL > lvlL) { lvlL = _lvlL; }
        if (_lvlR > lvlR) { lvlR = _lvlR; }
        ImGui::VolumeMeter(lvlL, lvlL, -60, 10);
        ImGui::VolumeMeter(lvlR, lvlR, -60, 10);

        if (ImGui::SliderFloat(CONCAT("##_recorder_vol_", name), &audioVolume, 0, 1, "")) {
            vol.setVolume(audioVolume);
        }
        ImGui::PopItemWidth();

        if (!folderSelect.pathIsValid() || selectedStreamName == "") { style::beginDisabled(); }
        if (!recording) {
            if (ImGui::Button(CONCAT("Record##_recorder_rec_", name), ImVec2(menuColumnWidth, 0))) {
                std::lock_guard lck(recMtx);
                startRecording();
            }
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
        }
        else {
            if (ImGui::Button(CONCAT("Stop##_recorder_rec_", name), ImVec2(menuColumnWidth, 0))) {
                std::lock_guard lck(recMtx);
                stopRecording();
            }
            uint64_t seconds = samplesWritten / (uint64_t)sampleRate;
            time_t diff = seconds;
            tm *dtm = gmtime(&diff);
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
        }
        if (!folderSelect.pathIsValid() || selectedStreamName == "") { style::endDisabled(); }
    }

    static void _audioHandler(dsp::stereo_t *data, int count, void *ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        volk_32f_s32f_convert_16i(_this->wavSampleBuf, (float*)data, 32767.0f, count*2);
        _this->audioWriter->writeSamples(_this->wavSampleBuf, count * 2 * sizeof(int16_t));
        _this->samplesWritten += count;
    }

    static void _basebandHandler(dsp::complex_t *data, int count, void *ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        volk_32f_s32f_convert_16i(_this->wavSampleBuf, (float*)data, 32767.0f, count*2);
        _this->basebandWriter->writeSamples(_this->wavSampleBuf, count * 2 * sizeof(int16_t));
        _this->samplesWritten += count;
    }

    static void moduleInterfaceHandler(int code, void* in, void* out, void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        std::lock_guard lck(_this->recMtx);
        if (code == RECORDER_IFACE_CMD_GET_MODE) {
            int* _out = (int*)out;
            *_out = _this->recMode;
        }
        else if (code == RECORDER_IFACE_CMD_SET_MODE) {
            if (_this->recording) { return; }
            int* _in = (int*)in;
            _this->recMode = std::clamp<int>(*_in, 0, 1);
        }
        else if (code == RECORDER_IFACE_CMD_START) {
            if (!_this->recording) { _this->startRecording(); }
        }
        else if (code == RECORDER_IFACE_CMD_STOP) {
            if (_this->recording) { _this->stopRecording(); }
        }
    }

    void startRecording() {
        if (recMode == RECORDER_MODE_BASEBAND) {
            samplesWritten = 0;
            std::string expandedPath = expandString(folderSelect.path + genFileName("/baseband_", false));
            sampleRate = sigpath::signalPath.getSampleRate();
            basebandWriter = new WavWriter(expandedPath, 16, 2, sigpath::signalPath.getSampleRate());
            if (basebandWriter->isOpen()) {
                basebandHandler.start();
                sigpath::signalPath.bindIQStream(&basebandStream);
                recording = true;
                spdlog::info("Recording to '{0}'", expandedPath);
            }
            else {
                spdlog::error("Could not create '{0}'", expandedPath);
            }
        }
        else if (recMode == RECORDER_MODE_AUDIO) {
            samplesWritten = 0;
            std::string expandedPath = expandString(folderSelect.path + genFileName("/audio_", true, selectedStreamName));
            sampleRate = sigpath::sinkManager.getStreamSampleRate(selectedStreamName);
            audioWriter = new WavWriter(expandedPath, 16, 2, sigpath::sinkManager.getStreamSampleRate(selectedStreamName));
            if (audioWriter->isOpen()) {
                recording = true;
                audioHandler.start();
                audioSplit.bindStream(&audioHandlerStream);
                spdlog::info("Recording to '{0}'", expandedPath);
            }
            else {
                spdlog::error("Could not create '{0}'", expandedPath);
            }
        }
    }

    void stopRecording() {
        if (recMode == 0) {
            recording = false;
            sigpath::signalPath.unbindIQStream(&basebandStream);
            basebandHandler.stop();
            basebandWriter->close();
            delete basebandWriter;
        }
        else if (recMode == 1) {
            recording = false;
            audioSplit.unbindStream(&audioHandlerStream);
            audioHandler.stop();
            audioWriter->close();
            delete audioWriter;
        }
    }


    std::string name;
    bool enabled = true;

    int recMode = 1;
    bool recording = false;

    float audioVolume = 1.0f;

    double sampleRate = 48000;

    float lvlL = -90.0f;
    float lvlR = -90.0f;

    dsp::stream<dsp::stereo_t> dummyStream;
    
    std::mutex recMtx;

    FolderSelect folderSelect;

    // Audio path
    dsp::stream<dsp::stereo_t>* audioInput = NULL;
    dsp::Volume<dsp::stereo_t> vol;
    dsp::Splitter<dsp::stereo_t> audioSplit;
    dsp::stream<dsp::stereo_t> meterStream;
    dsp::LevelMeter meter;
    dsp::stream<dsp::stereo_t> audioHandlerStream;
    dsp::HandlerSink<dsp::stereo_t> audioHandler;
    WavWriter* audioWriter;

    std::vector<std::string> streamNames;
    std::string streamNamesTxt;
    int streamId = 0;
    std::string selectedStreamName = "";

    // Baseband path
    dsp::stream<dsp::complex_t> basebandStream;
    dsp::HandlerSink<dsp::complex_t> basebandHandler;
    WavWriter* basebandWriter;

    uint64_t samplesWritten;
    int16_t* wavSampleBuf;

};

struct RecorderContext_t {
    std::string name;
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
    config.setPath(options::opts.root + "/recorder_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RecorderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* inst) {
    delete (RecorderModule*)inst;
}

MOD_EXPORT void _END_(RecorderContext_t* ctx) {
    config.disableAutoSave();
    config.save();
}