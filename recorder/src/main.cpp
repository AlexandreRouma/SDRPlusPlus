#include <imgui.h>
#include <module.h>
#include <watcher.h>
#include <wav.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <thread>
#include <ctime>
#include <gui/gui.h>
#include <filesystem>
#include <signal_path/signal_path.h>
#include <config.h>
#include <gui/style.h>
#include <regex>
#include <options.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "recorder",
    /* Description:     */ "Recorder module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 2,
    /* Max instances    */ -1
};

// TODO: Fix this and finish module

ConfigManager config;

std::string genFileName(std::string prefix) {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buf[1024];
    sprintf(buf, "%02d-%02d-%02d_%02d-%02d-%02d.wav", ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900);
    return prefix + buf;
}

void streamRemovedHandler(void* ctx) {

}

void sampleRateChanged(void* ctx, double sampleRate, int blockSize) {

}

std::string expandString(std::string input) {
    input = std::regex_replace(input, std::regex("%ROOT%"), options::opts.root);
    return std::regex_replace(input, std::regex("//"), "/");
}

class RecorderModule : public ModuleManager::Instance {
public:
    RecorderModule(std::string name) {
        this->name = name;
        recording = false;
        selectedStreamName = "";
        selectedStreamId = 0;
        lastNameList = "";

        config.aquire();
        if (!config.conf.contains(name)) {
            config.conf[name]["recMode"] = 1;
            config.conf[name]["directory"] = "%ROOT%/recordings";
        }
        recMode = config.conf[name]["recMode"];
        std::string recPath = config.conf[name]["directory"];
        strcpy(path, recPath.c_str());
        config.release(true);

        gui::menu.registerEntry(name, menuHandler, this);
    }

    ~RecorderModule() {

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
    static void menuHandler(void* ctx) {
        RecorderModule* _this = (RecorderModule*)ctx;
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

        std::vector<std::string> streamNames = sigpath::sinkManager.getStreamNames();
        std::string nameList;
        for (std::string name : streamNames) {
            nameList += name;
            nameList += '\0';
        }

        if (nameList == "") {
            ImGui::Text("No audio stream available");
            return;
        }

        if (_this->lastNameList != nameList) {
            _this->lastNameList = nameList;
            auto _nameIt = std::find(streamNames.begin(), streamNames.end(), _this->selectedStreamName);
            if (_nameIt == streamNames.end()) {
                // TODO: verify if there even is a stream
                _this->selectedStreamId = 0;
                _this->selectedStreamName = streamNames[_this->selectedStreamId];
            }
            else {
                _this->selectedStreamId = std::distance(streamNames.begin(), _nameIt);
                _this->selectedStreamName = streamNames[_this->selectedStreamId];
            }
        }

        ImGui::BeginGroup();
        if (_this->recording) { style::beginDisabled(); }
        ImGui::SetNextItemWidth(menuColumnWidth);
        bool lastPathValid = _this->pathValid;
        if (!lastPathValid) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        }
        if (ImGui::InputText(CONCAT("##_recorder_path_", _this->name), _this->path, 4095)) {
            std::string expandedPath = expandString(_this->path);
            if (!std::filesystem::exists(expandedPath)) {
                _this->pathValid = false;
            }
            else if (!std::filesystem::is_directory(expandedPath)) {
                _this->pathValid = false;
            }
            else {
                _this->pathValid = true;
                config.aquire();
                config.conf[_this->name]["directory"] = _this->path;
                config.release(true);
            }
        }
        if (!lastPathValid) {
            ImGui::PopStyleColor();
        }

        // TODO: Change VFO ref in signal path
        // TODO: Add VFO record
        ImGui::Columns(2, CONCAT("RecordModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("Baseband##_", _this->name), _this->recMode == 0) && _this->recMode != 0) { 
            _this->recMode = 0;
            config.aquire();
            config.conf[_this->name]["recMode"] = _this->recMode;
            config.release(true);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("Audio##_", _this->name), _this->recMode == 1) && _this->recMode != 1) {
            _this->recMode = 1;
            config.aquire();
            config.conf[_this->name]["recMode"] = _this->recMode;
            config.release(true);
        }
        if (_this->recording) { style::endDisabled(); }
        ImGui::Columns(1, CONCAT("EndRecordModeColumns##_", _this->name), false);

        ImGui::EndGroup();

        if (_this->recMode == 0) {
            ImGui::PushItemWidth(menuColumnWidth);
            if (!_this->recording) {
                if (!_this->pathValid) { style::beginDisabled(); }
                if (ImGui::Button("Record", ImVec2(menuColumnWidth, 0))) {
                    std::string expandedPath = expandString(std::string(_this->path) + genFileName("/baseband_"));
                    _this->samplesWritten = 0;
                    _this->sampleRate = sigpath::signalPath.getSampleRate();
                    _this->writer = new WavWriter(expandedPath, 16, 2, _this->sampleRate);
                    _this->iqStream = new dsp::stream<dsp::complex_t>;
                    sigpath::signalPath.bindIQStream(_this->iqStream);
                    _this->workerThread = std::thread(_iqWriteWorker, _this);
                    _this->recording = true;
                    _this->startTime = time(0);
                }
                if (!_this->pathValid) { style::endDisabled(); }
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
            }
            else {
                if (ImGui::Button("Stop", ImVec2(menuColumnWidth, 0))) {
                    _this->iqStream->stopReader();
                    _this->workerThread.join();
                    _this->iqStream->clearReadStop();
                    sigpath::signalPath.unbindIQStream(_this->iqStream);
                    _this->writer->close();
                    delete _this->writer;
                    _this->recording = false;
                }
                uint64_t seconds = _this->samplesWritten / (uint64_t)_this->sampleRate;
                time_t diff = seconds;
                tm *dtm = gmtime(&diff);
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
            }
        }
        else if (_this->recMode == 1) {
            ImGui::PushItemWidth(menuColumnWidth);
            if (!_this->recording) {
                if (ImGui::Combo(CONCAT("##_strea_select_", _this->name), &_this->selectedStreamId, nameList.c_str())) {
                    _this->selectedStreamName = streamNames[_this->selectedStreamId];
                }
            }
            else {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.44f, 0.44f, 0.44f, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.21f, 0.22f, 0.30f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 0.65f));
                ImGui::Combo(CONCAT("##_strea_select_", _this->name), &_this->selectedStreamId, nameList.c_str());
                ImGui::PopItemFlag();
                ImGui::PopStyleColor(3);
            }
            if (!_this->recording) {
                if (!_this->pathValid) { style::beginDisabled(); }
                if (ImGui::Button("Record", ImVec2(menuColumnWidth, 0))) {
                    std::string expandedPath = expandString(std::string(_this->path) + genFileName("/audio_"));
                    _this->samplesWritten = 0;
                    _this->sampleRate = sigpath::sinkManager.getStreamSampleRate(_this->selectedStreamName);
                    _this->writer = new WavWriter(expandedPath, 16, 2, _this->sampleRate);
                    _this->audioStream = sigpath::sinkManager.bindStream(_this->selectedStreamName);
                    _this->workerThread = std::thread(_audioWriteWorker, _this);
                    _this->recording = true;
                    _this->startTime = time(0);
                }
                if (!_this->pathValid) { style::endDisabled(); }
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
            }
            else {
                if (ImGui::Button("Stop", ImVec2(menuColumnWidth, 0))) {
                    _this->audioStream->stopReader();
                    _this->workerThread.join();
                    _this->audioStream->clearReadStop();
                    sigpath::sinkManager.unbindStream(_this->selectedStreamName, _this->audioStream);
                    _this->writer->close();
                    delete _this->writer;
                    _this->recording = false;
                }
                uint64_t seconds = _this->samplesWritten / (uint64_t)_this->sampleRate;
                time_t diff = seconds;
                tm *dtm = gmtime(&diff);
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
            }
        }
    }

    static void _audioWriteWorker(RecorderModule* _this) {
        int16_t* sampleBuf = new int16_t[STREAM_BUFFER_SIZE * 2];
        while (true) {
            int count = _this->audioStream->read();
            if (count < 0) { break; }
            for (int i = 0; i < count; i++) {
                sampleBuf[(i * 2) + 0] = _this->audioStream->data[i].l * 0x7FFF;
                sampleBuf[(i * 2) + 1] = _this->audioStream->data[i].r * 0x7FFF;
            }
            _this->audioStream->flush();
            _this->samplesWritten += count;
            _this->writer->writeSamples(sampleBuf, count * sizeof(int16_t) * 2);
        }
        delete[] sampleBuf;
    }

    static void _iqWriteWorker(RecorderModule* _this) {
        int16_t* sampleBuf = new int16_t[STREAM_BUFFER_SIZE];
        while (true) {
            int count = _this->iqStream->read();
            if (count < 0) { break; }
            for (int i = 0; i < count; i++) {
                sampleBuf[(i * 2) + 0] = _this->iqStream->data[i].q * 0x7FFF;
                sampleBuf[(i * 2) + 1] = _this->iqStream->data[i].i * 0x7FFF;
            }
            _this->iqStream->flush();
            _this->samplesWritten += count;
            _this->writer->writeSamples(sampleBuf, count * sizeof(int16_t) * 2);
        }
        delete[] sampleBuf;
    }

    std::string name;
    bool enabled = true;
    char path[4096];
    bool pathValid = true;
    dsp::stream<dsp::stereo_t>* audioStream;
    dsp::stream<dsp::complex_t>* iqStream;
    WavWriter* writer;
    VFOManager::VFO* vfo;
    std::thread workerThread;
    bool recording;
    time_t startTime;
    std::string lastNameList;
    std::string selectedStreamName;
    int selectedStreamId;
    uint64_t samplesWritten;
    float sampleRate;
    float vfoSampleRate = 200000;
    int recMode = 1;

};

struct RecorderContext_t {
    std::string name;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(options::opts.root + "/recorder_config.json");
    config.load(def);
    config.enableAutoSave();

    // Create default recording directory
    if (!std::filesystem::exists(options::opts.root + "/recordings")) {
        spdlog::warn("Recordings directory does not exist, creating it");
        if (!std::filesystem::create_directory(options::opts.root + "/recordings")) {
            spdlog::error("Could not create recordings directory");
        }
    }
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