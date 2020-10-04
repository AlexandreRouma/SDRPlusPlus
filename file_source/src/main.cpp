#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <wavreader.h>
#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

MOD_INFO {
    /* Name:        */ "fike_source",
    /* Description: */ "File input module for SDR++",
    /* Author:      */ "Ryzerth",
    /* Version:     */ "0.1.0"
};

class FileSourceModule {
public:
    FileSourceModule(std::string name) {
        this->name = name;

        stream.init(100);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("File", &handler);

        reader = new WavReader("D:/satpic/raw_recordings/NOAA-18_09-08-2018_21-39-00_baseband_NR.wav");
        
        spdlog::info("Samplerate: {0}, Bit depth: {1}, Channel count: {2}", reader->getSampleRate(), reader->getBitDepth(), reader->getChannelCount());

        spdlog::info("FileSourceModule '{0}': Instance created!", name);
    }

    ~FileSourceModule() {
        spdlog::info("FileSourceModule '{0}': Instance deleted!", name);
    }

private:
    static void menuSelected(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        core::setInputSampleRate(_this->reader->getSampleRate());
        spdlog::info("FileSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        spdlog::info("FileSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        _this->workerThread = std::thread(worker, _this);
        spdlog::info("FileSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        spdlog::info("FileSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        spdlog::info("FileSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        ImGui::Text("Hi from %s!", _this->name.c_str());
    }

    static void worker(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        double sampleRate = _this->reader->getSampleRate();
        int blockSize = sampleRate / 200.0;
        int16_t* inBuf = new int16_t[blockSize * 2];
        dsp::complex_t* outBuf = new dsp::complex_t[blockSize];

        _this->stream.setMaxLatency(blockSize * 2);

        while (true) {
            _this->reader->readSamples(inBuf, blockSize * 2 * sizeof(int16_t));
            for (int i = 0; i < blockSize; i++) {
                outBuf[i].q = (float)inBuf[i * 2] / (float)0x7FFF;
                outBuf[i].i = (float)inBuf[(i * 2) + 1] / (float)0x7FFF;
            }
            if (_this->stream.write(outBuf, blockSize) < 0) { break; };
            //std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        delete[] inBuf;
        delete[] outBuf;
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    WavReader* reader;
    std::thread workerThread;
};

MOD_EXPORT void _INIT_() {
   // Do your one time init here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new FileSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (FileSourceModule*)instance;
}

MOD_EXPORT void _STOP_() {
    // Do your one shutdown here
}