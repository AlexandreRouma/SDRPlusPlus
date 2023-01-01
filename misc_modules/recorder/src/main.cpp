#include <imgui.h>
#include <module.h>
#include <wav.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <dsp/bench/peak_level_meter.h>
#include <dsp/sink/handler_sink.h>
#include <dsp/routing/splitter.h>
#include <dsp/audio/volume.h>
#include <dsp/convert/stereo_to_mono.h>
#include <thread>
#include <ctime>
#include <gui/gui.h>
#include <filesystem>
#include <signal_path/signal_path.h>
#include <config.h>
#include <gui/style.h>
#include <gui/widgets/volume_meter.h>
#include <regex>
#include <gui/widgets/folder_select.h>
#include <recorder_interface.h>
#include <core.h>
#include <utils/optionlist.h>
#include "wav.h"

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "recorder",
    /* Description:     */ "Recorder module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 3, 0,
    /* Max instances    */ -1
};

ConfigManager config;

class RecorderModule : public ModuleManager::Instance {
public:
    RecorderModule(std::string name) : folderSelect("%ROOT%/recordings") {
        this->name = name;
        root = (std::string)core::args["root"];

        // Define option lists
        containers.define("WAV", wav::FORMAT_WAV);
        // containers.define("RF64", wav::FORMAT_RF64); // Disabled for now
        sampleTypes.define(wav::SAMP_TYPE_UINT8, "Uint8", wav::SAMP_TYPE_UINT8);
        sampleTypes.define(wav::SAMP_TYPE_INT16, "Int16", wav::SAMP_TYPE_INT16);
        sampleTypes.define(wav::SAMP_TYPE_INT32, "Int32", wav::SAMP_TYPE_INT32);
        sampleTypes.define(wav::SAMP_TYPE_FLOAT32, "Float32", wav::SAMP_TYPE_FLOAT32);

        // Load default config for option lists
        containerId = containers.valueId(wav::FORMAT_WAV);
        sampleTypeId = sampleTypes.valueId(wav::SAMP_TYPE_INT16);

        // Load config
        config.acquire();
        if (config.conf[name].contains("mode")) {
            recMode = config.conf[name]["mode"];
        }
        if (config.conf[name].contains("recPath")) {
            folderSelect.setPath(config.conf[name]["recPath"]);
        }
        if (config.conf[name].contains("format") && containers.keyExists(config.conf[name]["format"])) {
            containerId = containers.keyId(config.conf[name]["format"]);
        }
        if (config.conf[name].contains("sampleType") && sampleTypes.keyExists(config.conf[name]["sampleType"])) {
            sampleTypeId = sampleTypes.keyId(config.conf[name]["sampleType"]);
        }
        if (config.conf[name].contains("audioStream")) {
            selectedStreamName = config.conf[name]["audioStream"];
        }
        if (config.conf[name].contains("audioVolume")) {
            audioVolume = config.conf[name]["audioVolume"];
        }
        if (config.conf[name].contains("ignoreSilence")) {
            ignoreSilence = config.conf[name]["ignoreSilence"];
        }
        config.release();

        // Init audio path
        volume.init(NULL, audioVolume, false);
        splitter.init(&volume.out);
        splitter.bindStream(&meterStream);
        meter.init(&meterStream);
        s2m.init(NULL);

        // Init sinks
        basebandSink.init(NULL, complexHandler, this);
        stereoSink.init(NULL, stereoHandler, this);
        monoSink.init(&s2m.out, monoHandler, this);

        gui::menu.registerEntry(name, menuHandler, this);
    }

    ~RecorderModule() {
        stop();
        deselectStream();
        meter.stop();
        gui::menu.removeEntry(name);
    }

    void postInit() {
        // Enumerate streams
        audioStreams.clear();
        auto names = sigpath::sinkManager.getStreamNames();
        for (const auto& name : names) {
            audioStreams.define(name, name, name);
        }

        // Bind stream register/unregister handlers
        onStreamRegisteredHandler.ctx = this;
        onStreamRegisteredHandler.handler = streamRegisteredHandler;
        sigpath::sinkManager.onStreamRegistered.bindHandler(&onStreamRegisteredHandler);
        onStreamUnregisterHandler.ctx = this;
        onStreamUnregisterHandler.handler = streamUnregisterHandler;
        sigpath::sinkManager.onStreamUnregister.bindHandler(&onStreamUnregisterHandler);

        // Select the stream
        selectStream(selectedStreamName);
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

    void start() {
        std::lock_guard<std::recursive_mutex> lck(recMtx);
        if (recording) { return; }

        // Configure the wav writer
        if (recMode == RECORDER_MODE_AUDIO) {
            if (selectedStreamName.empty()) { return; }
            samplerate = sigpath::sinkManager.getStreamSampleRate(selectedStreamName);
        }
        else {
            samplerate = sigpath::iqFrontEnd.getSampleRate();
        }
        writer.setFormat(containers[containerId]);
        writer.setChannels((recMode == RECORDER_MODE_AUDIO && !stereo) ? 1 : 2);
        writer.setSampleType(sampleTypes[sampleTypeId]);
        writer.setSamplerate(samplerate);

        // Open file
        std::string prefix = (recMode == RECORDER_MODE_AUDIO) ? "/audio_" : "/baseband_";
        std::string expandedPath = expandString(folderSelect.path + genFileName(prefix, false));
        if (!writer.open(expandedPath)) {
            spdlog::error("Failed to open file for recording: {0}", expandedPath);
            return;
        }

        // Open audio stream or baseband
        if (recMode == RECORDER_MODE_AUDIO) {
            // TODO: Select the stereo to mono converter if needed
            stereoStream = sigpath::sinkManager.bindStream(selectedStreamName);
            if (stereo) {
                stereoSink.setInput(stereoStream);
                stereoSink.start();
            }
            else {
                s2m.setInput(stereoStream);
                s2m.start();
                monoSink.start();
            }
        }
        else {
            // Create and bind IQ stream
            basebandStream = new dsp::stream<dsp::complex_t>();
            basebandSink.setInput(basebandStream);
            basebandSink.start();
            sigpath::iqFrontEnd.bindIQStream(basebandStream);
        }

        recording = true;
    }

    void stop() {
        std::lock_guard<std::recursive_mutex> lck(recMtx);
        if (!recording) { return; }

        // Close audio stream or baseband
        if (recMode == RECORDER_MODE_AUDIO) {
            // NOTE: Has to be done before the unbind since the stream is deleted...
            monoSink.stop();
            stereoSink.stop();
            s2m.stop();
            sigpath::sinkManager.unbindStream(selectedStreamName, stereoStream);
        }
        else {
            // Unbind and destroy IQ stream
            sigpath::iqFrontEnd.unbindIQStream(basebandStream);
            basebandSink.stop();
            delete basebandStream;
        }

        // Close file
        writer.close();
        
        recording = false;
    }

private:
    static void menuHandler(void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        // Recording mode
        if (_this->recording) { style::beginDisabled(); }
        ImGui::BeginGroup();
        ImGui::Columns(2, CONCAT("RecorderModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("Baseband##_recorder_mode_", _this->name), _this->recMode == RECORDER_MODE_BASEBAND)) {
            _this->recMode = RECORDER_MODE_BASEBAND;
            config.acquire();
            config.conf[_this->name]["mode"] = _this->recMode;
            config.release(true);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("Audio##_recorder_mode_", _this->name), _this->recMode == RECORDER_MODE_AUDIO)) {
            _this->recMode = RECORDER_MODE_AUDIO;
            config.acquire();
            config.conf[_this->name]["mode"] = _this->recMode;
            config.release(true);
        }
        ImGui::Columns(1, CONCAT("EndRecorderModeColumns##_", _this->name), false);
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

        ImGui::LeftLabel("Container");
        ImGui::FillWidth();
        if (ImGui::Combo(CONCAT("##_recorder_container_", _this->name), &_this->containerId, _this->containers.txt)) {
            config.acquire();
            config.conf[_this->name]["container"] = _this->containers.key(_this->containerId);
            config.release(true);
        }

        ImGui::LeftLabel("Sample type");
        ImGui::FillWidth();
        if (ImGui::Combo(CONCAT("##_recorder_st_", _this->name), &_this->sampleTypeId, _this->sampleTypes.txt)) {
            config.acquire();
            config.conf[_this->name]["sampleType"] = _this->sampleTypes.key(_this->sampleTypeId);
            config.release(true);
        }

        // Show additional audio options
        if (_this->recMode == RECORDER_MODE_AUDIO) {
            ImGui::LeftLabel("Stream");
            ImGui::FillWidth();
            if (ImGui::Combo(CONCAT("##_recorder_stream_", _this->name), &_this->streamId, _this->audioStreams.txt)) {
                _this->selectStream(_this->audioStreams.value(_this->streamId));
                config.acquire();
                config.conf[_this->name]["audioStream"] = _this->audioStreams.key(_this->streamId);
                config.release(true);
            }

            _this->updateAudioMeter(_this->audioLvl);
            ImGui::FillWidth();
            ImGui::VolumeMeter(_this->audioLvl.l, _this->audioLvl.l, -60, 10);
            ImGui::FillWidth();
            ImGui::VolumeMeter(_this->audioLvl.r, _this->audioLvl.r, -60, 10);

            ImGui::FillWidth();
            if (ImGui::SliderFloat(CONCAT("##_recorder_vol_", _this->name), &_this->audioVolume, 0, 1, "")) {
                _this->volume.setVolume(_this->audioVolume);
                config.acquire();
                config.conf[_this->name]["audioVolume"] = _this->audioVolume;
                config.release(true);
            }

            if (_this->recording) { style::beginDisabled(); }
            if (ImGui::Checkbox(CONCAT("Stereo##_recorder_stereo_", _this->name), &_this->stereo)) {
                config.acquire();
                config.conf[_this->name]["stereo"] = _this->stereo;
                config.release(true);
            }
            if (_this->recording) { style::endDisabled(); }

            if (ImGui::Checkbox(CONCAT("Ignore silence##_recorder_ignore_silence_", _this->name), &_this->ignoreSilence)) {
                config.acquire();
                config.conf[_this->name]["ignoreSilence"] = _this->ignoreSilence;
                config.release(true);
            }
        }

        // Record button
        bool canRecord = _this->folderSelect.pathIsValid();
        if (_this->recMode == RECORDER_MODE_AUDIO) { canRecord &= !_this->selectedStreamName.empty(); }
        if (!_this->recording) {
            if (ImGui::Button(CONCAT("Record##_recorder_rec_", _this->name), ImVec2(menuWidth, 0))) {
                _this->start();
            }
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
        }
        else {
            if (ImGui::Button(CONCAT("Stop##_recorder_rec_", _this->name), ImVec2(menuWidth, 0))) {
                _this->stop();
            }
            uint64_t seconds = _this->writer.getSamplesWritten() / _this->samplerate;
            time_t diff = seconds;
            tm* dtm = gmtime(&diff);
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
        }
    }

    void selectStream(std::string name) {
        std::lock_guard<std::recursive_mutex> lck(recMtx);
        deselectStream();

        if (audioStreams.empty()) {
            selectedStreamName.clear();
            return;
        }
        else if (!audioStreams.keyExists(name)) {
            selectStream(audioStreams.key(0));
            return;
        }

        audioStream = sigpath::sinkManager.bindStream(name);
        if (!audioStream) { return; }
        selectedStreamName = name;
        streamId = audioStreams.keyId(name);
        volume.setInput(audioStream);
        startAudioPath();
    }

    void deselectStream() {
        std::lock_guard<std::recursive_mutex> lck(recMtx);
        if (selectedStreamName.empty() || !audioStream) {
            selectedStreamName.clear();
            return;
        }
        if (recording && recMode == RECORDER_MODE_AUDIO) { stop(); }
        stopAudioPath();
        sigpath::sinkManager.unbindStream(selectedStreamName, audioStream);
        selectedStreamName.clear();
        audioStream = NULL;
    }

    void startAudioPath() {
        volume.start();
        splitter.start();
        meter.start();
    }

    void stopAudioPath() {
        volume.stop();
        splitter.stop();
        meter.stop();
    }

    static void streamRegisteredHandler(std::string name, void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;

        // Add new stream to the list
        _this->audioStreams.define(name, name, name);

        // If no stream is selected, select new stream. If not, update the menu ID. 
        if (_this->selectedStreamName.empty()) {
            _this->selectStream(name);
        }
        else {
            _this->streamId = _this->audioStreams.keyId(_this->selectedStreamName);
        }
    }

    static void streamUnregisterHandler(std::string name, void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;

        // Remove stream from list
        _this->audioStreams.undefineKey(name);

        // If the stream is in used, deselect it and reselect default. Otherwise, update ID.
        if (_this->selectedStreamName == name) {
            _this->selectStream("");
        }
        else {
            _this->streamId = _this->audioStreams.keyId(_this->selectedStreamName);
        }
    }

    void updateAudioMeter(dsp::stereo_t& lvl) {
        // Note: Yes, using the natural log is on purpose, it just gives a more beautiful result.
        double frameTime = 1.0 / ImGui::GetIO().Framerate;
        lvl.l = std::clamp<float>(lvl.l - (frameTime * 50.0), -90.0f, 10.0f);
        lvl.r = std::clamp<float>(lvl.r - (frameTime * 50.0), -90.0f, 10.0f);
        // TODO: FINISH METER
        dsp::stereo_t rawLvl = meter.getLevel();
        meter.resetLevel();
        dsp::stereo_t dbLvl = { 10.0f * logf(rawLvl.l), 10.0f * logf(rawLvl.r) };
        if (dbLvl.l > lvl.l) { lvl.l = dbLvl.l; }
        if (dbLvl.r > lvl.r) { lvl.r = dbLvl.r; }
    }

    // TODO: REPLACE WITH SOMETHING CLEAN
    std::string genFileName(std::string prefix, bool isVfo, std::string name = "") {
        time_t now = time(0);
        tm* ltm = localtime(&now);
        char buf[1024];
        double freq = gui::waterfall.getCenterFrequency();
        ;
        if (isVfo && gui::waterfall.vfos.find(name) != gui::waterfall.vfos.end()) {
            freq += gui::waterfall.vfos[name]->generalOffset;
        }
        sprintf(buf, "%.0lfHz_%02d-%02d-%02d_%02d-%02d-%02d.wav", freq, ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900);
        return prefix + buf;
    }
    std::string expandString(std::string input) {
        input = std::regex_replace(input, std::regex("%ROOT%"), root);
        return std::regex_replace(input, std::regex("//"), "/");
    }

    static void complexHandler(dsp::complex_t* data, int count, void* ctx) {
        monoHandler((float*)data, count, ctx);
    }

    static void stereoHandler(dsp::stereo_t* data, int count, void* ctx) {
        monoHandler((float*)data, count, ctx);
    }

    static void monoHandler(float* data, int count, void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        _this->writer.write(data, count);
    }

    std::string name;
    bool enabled = true;
    std::string root;

    OptionList<std::string, wav::Format> containers;
    OptionList<int, wav::SampleType> sampleTypes;
    FolderSelect folderSelect;

    int recMode = RECORDER_MODE_AUDIO;
    int containerId;
    int sampleTypeId;
    bool stereo = true;
    std::string selectedStreamName = "";
    float audioVolume = 1.0f;
    bool ignoreSilence = false;
    dsp::stereo_t audioLvl = { -100.0f, -100.0f };

    bool recording = false;
    wav::Writer writer;
    std::recursive_mutex recMtx;
    dsp::stream<dsp::complex_t>* basebandStream;
    dsp::stream<dsp::stereo_t>* stereoStream;
    dsp::sink::Handler<dsp::complex_t> basebandSink;
    dsp::sink::Handler<dsp::stereo_t> stereoSink;
    dsp::sink::Handler<float> monoSink;

    OptionList<std::string, std::string> audioStreams;
    int streamId = 0;
    dsp::stream<dsp::stereo_t>* audioStream = NULL;
    dsp::audio::Volume volume;
    dsp::routing::Splitter<dsp::stereo_t> splitter;
    dsp::stream<dsp::stereo_t> meterStream;
    dsp::bench::PeakLevelMeter<dsp::stereo_t> meter;
    dsp::convert::StereoToMono s2m;

    uint64_t samplerate = 48000;

    EventHandler<std::string> onStreamRegisteredHandler;
    EventHandler<std::string> onStreamUnregisterHandler;

};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    std::string root = (std::string)core::args["root"];
    if (!std::filesystem::exists(root + "/recordings")) {
        spdlog::warn("Recordings directory does not exist, creating it");
        if (!std::filesystem::create_directory(root + "/recordings")) {
            spdlog::error("Could not create recordings directory");
        }
    }
    json def = json({});
    config.setPath(root + "/recorder_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RecorderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* inst) {
    delete (RecorderModule*)inst;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}