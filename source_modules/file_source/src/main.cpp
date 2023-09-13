#define NOMINMAX
#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <wavreader.h>
#include <core.h>
#include <gui/widgets/file_select.h>
#include <filesystem>
#include <regex>
#include <gui/tuner.h>
#include <algorithm>
#include <stdexcept>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "file_source",
    /* Description:     */ "Wav file source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 1,
    /* Max instances    */ 1
};

ConfigManager config;

class FileSourceModule : public ModuleManager::Instance {
public:
    FileSourceModule(std::string name) : fileSelect("", { "Wav IQ Files (*.wav)", "*.wav", "All Files", "*" }) {
        this->name = name;

        if (core::args["server"].b()) { return; }

        config.acquire();
        fileSelect.setPath(config.conf["path"], true);
        config.release();

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("File", &handler);
    }

    ~FileSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("File");
    }

    void postInit() {}

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
    static void menuSelected(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        tuner::tune(tuner::TUNER_MODE_IQ_ONLY, "", _this->centerFreq);
        sigpath::iqFrontEnd.setBuffering(false);
        gui::waterfall.centerFrequencyLocked = true;
        //gui::freqSelect.minFreq = _this->centerFreq - (_this->sampleRate/2);
        //gui::freqSelect.maxFreq = _this->centerFreq + (_this->sampleRate/2);
        //gui::freqSelect.limitFreq = true;
        flog::info("FileSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        sigpath::iqFrontEnd.setBuffering(true);
        //gui::freqSelect.limitFreq = false;
        gui::waterfall.centerFrequencyLocked = false;
        flog::info("FileSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->reader == NULL) { return; }
        _this->running = true;
        _this->workerThread = _this->float32Mode ? std::thread(floatWorker, _this) : std::thread(worker, _this);
        flog::info("FileSourceModule '{0}': Start!", _this->name);

        secondsToTime(_this->reader->getLength(), _this->totalHours, _this->totalMinutes, _this->totalSeconds);
    }

    static void stop(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        if (!_this->running) { return; }
        if (_this->reader == NULL) { return; }
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        _this->running = false;
        _this->reader->rewind();
        flog::info("FileSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        flog::info("FileSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;

        if (_this->fileSelect.render("##file_source_" + _this->name)) {
            if (_this->fileSelect.pathIsValid()) {
                if (_this->reader != NULL) {
                    _this->reader->close();
                    delete _this->reader;
                }
                try {
                    _this->reader = new WavReader(_this->fileSelect.path);
                    if (_this->reader->getSampleRate() == 0) {
                        _this->reader->close();
                        delete _this->reader;
                        _this->reader = NULL;
                        throw std::runtime_error("Sample rate may not be zero");
                    }
                    _this->sampleRate = _this->reader->getSampleRate();
                    core::setInputSampleRate(_this->sampleRate);
                    std::string filename = std::filesystem::path(_this->fileSelect.path).filename().string();
                    _this->centerFreq = _this->getFrequency(filename);
                    tuner::tune(tuner::TUNER_MODE_IQ_ONLY, "", _this->centerFreq);
                    //gui::freqSelect.minFreq = _this->centerFreq - (_this->sampleRate/2);
                    //gui::freqSelect.maxFreq = _this->centerFreq + (_this->sampleRate/2);
                    //gui::freqSelect.limitFreq = true;
                }
                catch (std::exception& e) {
                    flog::error("Error: {0}", e.what());
                }
                config.acquire();
                config.conf["path"] = _this->fileSelect.path;
                config.release(true);
            }
        }

        if (_this->running) {
            uint32_t hours, minutes, seconds;
            secondsToTime(_this->reader->getPosition(), hours, minutes, seconds);
            // snprintf(_this->playTimeBuf, 32, "02%d:02%d:02%d / 02%d:02%d:02%d",
            //     _this->totalHours, _this->totalMinutes, _this->totalSeconds,
            //     hours, minutes, seconds
            // );
            ImGui::Text("%02d:%02d:%02d / %02d:%02d:%02d",
                hours, minutes, seconds,
                _this->totalHours, _this->totalMinutes, _this->totalSeconds
            );

            ImGui::BeginGroup();
            ImGui::Columns(6, "RewindButtons", false);
            
            if (ImGui::Button("<< 30s")) {
                _this->reader->rewindBySeconds(-30);
            }
            if (ImGui::Button("<< 30m")) {
                _this->reader->rewindBySeconds(-1800);
            }
            
            ImGui::NextColumn();
            if (ImGui::Button("<< 10s")) {
                _this->reader->rewindBySeconds(-10);
            }
            if (ImGui::Button("<< 10m")) {
                _this->reader->rewindBySeconds(-600);
            }

            ImGui::NextColumn();
            if (ImGui::Button("<< 5s")) {
                _this->reader->rewindBySeconds(-5);
            }
            if (ImGui::Button("<< 5m")) {
                _this->reader->rewindBySeconds(-300);
            }

            ImGui::NextColumn();
            if (ImGui::Button("5s >>")) {
                _this->reader->rewindBySeconds(5);
            }
            if (ImGui::Button("5m >>")) {
                _this->reader->rewindBySeconds(300);
            }

            ImGui::NextColumn();
            if (ImGui::Button("10s >>")) {
                _this->reader->rewindBySeconds(10);
            }
            if (ImGui::Button("10m >>")) {
                _this->reader->rewindBySeconds(600);
            }

            ImGui::NextColumn();
            if (ImGui::Button("30s >>")) {
                _this->reader->rewindBySeconds(30);
            }
            if (ImGui::Button("30m >>")) {
                _this->reader->rewindBySeconds(1800);
            }

            ImGui::Columns(1, "EndRewindButtons", false);
            ImGui::EndGroup();
        }

        ImGui::Checkbox("Float32 Mode##_file_source", &_this->float32Mode);
    }

    static void worker(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        double sampleRate = std::max(_this->reader->getSampleRate(), (uint32_t)1);
        int blockSize = std::min((int)(sampleRate / 200.0f), (int)STREAM_BUFFER_SIZE);
        int16_t* inBuf = new int16_t[blockSize * 2];

        while (true) {
            _this->reader->readSamples(inBuf, blockSize * 2 * sizeof(int16_t));
            volk_16i_s32f_convert_32f((float*)_this->stream.writeBuf, inBuf, 32768.0f, blockSize * 2);
            if (!_this->stream.swap(blockSize)) { break; };
        }

        delete[] inBuf;
    }

    static void floatWorker(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        double sampleRate = std::max(_this->reader->getSampleRate(), (uint32_t)1);
        int blockSize = std::min((int)(sampleRate / 200.0f), (int)STREAM_BUFFER_SIZE);
        dsp::complex_t* inBuf = new dsp::complex_t[blockSize];

        while (true) {
            _this->reader->readSamples(_this->stream.writeBuf, blockSize * sizeof(dsp::complex_t));
            if (!_this->stream.swap(blockSize)) { break; };
        }

        delete[] inBuf;
    }

    double getFrequency(std::string filename) {
        std::regex expr("[0-9]+Hz");
        std::smatch matches;
        std::regex_search(filename, matches, expr);
        if (matches.empty()) { return 0; }
        std::string freqStr = matches[0].str();
        return std::atof(freqStr.substr(0, freqStr.size() - 2).c_str());
    }

    static void secondsToTime(uint32_t seconds, uint32_t& hours, uint32_t& minutes, uint32_t& remainingSeconds) {
        remainingSeconds = seconds;
        hours = remainingSeconds / 3600;
        remainingSeconds %= 3600;
        minutes = remainingSeconds / 60;
        remainingSeconds %= 60;
    }

    FileSelect fileSelect;
    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    WavReader* reader = NULL;
    bool running = false;
    bool enabled = true;
    float sampleRate = 1000000;
    std::thread workerThread;

    double centerFreq = 100000000;

    bool float32Mode = false;

    uint32_t totalHours;
    uint32_t totalMinutes;
    uint32_t totalSeconds;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["path"] = "";
    config.setPath(core::args["root"].s() + "/file_source_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new FileSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (FileSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
