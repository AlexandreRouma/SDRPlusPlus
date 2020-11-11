#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>

#include <openFX3.h>
#include <Si5351.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

#define SEL0 (8)  		//   SEL0  GPIO26
#define SEL1 (16) 		//   SEL1  GPIO27

MOD_INFO {
    /* Name:        */ "rx888_source",
    /* Description: */ "RX888 input module for SDR++",
    /* Author:      */ "Ryzerth",
    /* Version:     */ "0.1.0"
};

class RX888SourceModule {
public:
    RX888SourceModule(std::string name) {
        this->name = name;

        if (!openFX3()) {
            spdlog::error("No RX888 found!");
            return;
        }

        Si5351init();

        sampleRate = 8000000;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("RX888", &handler);

        spdlog::info("RX888SourceModule '{0}': Instance created!", name);
    }

    ~RX888SourceModule() {
        spdlog::info("RX888SourceModule '{0}': Instance deleted!", name);
    }

private:

    static void menuSelected(void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        spdlog::info("RX888SourceModule '{0}': Menu Select!", _this->name);
        
        core::setInputSampleRate(_this->sampleRate);
    }

    static void menuDeselected(void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        spdlog::info("RX888SourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;

        _this->doStart();

        spdlog::info("RX888SourceModule '{0}': Start!", _this->name);
    }

    void doStart() {
        uint8_t Bgpio[2];
        Bgpio[0] = 0x17 | SEL0 & (~SEL1);
	    Bgpio[1] = 0x00;

        si5351aSetFrequency(sampleRate * 2, 0);
        fx3Control(GPIOFX3, Bgpio);

        running = true;
        workerThread = std::thread(_worker, this);
    }
    
    static void stop(void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        _this->running = false;
        _this->workerThread.join();
        spdlog::info("RX888SourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        _this->freq = freq;
        
        spdlog::info("RX888SourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    
    static void menuHandler(void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        
        ImGui::Text("RX888 source");
    }

    static void _worker(RX888SourceModule* _this) {
        int blockSize = _this->sampleRate / 200.0f;
        if ((blockSize % 2) != 0) { blockSize++; }
        int flags = 0;
        long long timeMs = 0;

        int16_t* buffer = new int16_t[blockSize * 2];

        long pktSize = EndPt->MaxPktSize;
        EndPt->SetXferSize(blockSize * 2);
        long ppx = blockSize * 2 / pktSize;

        OVERLAPPED inOvLap;
        inOvLap.hEvent = CreateEvent(NULL, false, false, NULL);
        auto context = EndPt->BeginDataXfer((PUCHAR)buffer, blockSize, &inOvLap);
        
        fx3Control(STARTFX3);
        
        while (_this->running) {
            //if (_this->stream.aquire() < 0) { break; }
            

            LONG rLen = blockSize * 2;
            if (!EndPt->WaitForXfer(&inOvLap, 5000)) {
                spdlog::error("Transfer aborted");
                EndPt->Abort();
                if (EndPt->LastError == ERROR_IO_PENDING) {
                    WaitForSingleObject(inOvLap.hEvent, 5000);
                }
                break;
            }

            if (EndPt->Attributes == 2) {
                if (EndPt->FinishDataXfer((PUCHAR)buffer, rLen, &inOvLap, context)) {
                    spdlog::warn("{0}", rLen);
                }
            }

            context = EndPt->BeginDataXfer((PUCHAR)buffer, blockSize, &inOvLap);
        }

        delete[] buffer;
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    double freq;
    double sampleRate;
    bool running = false;
};

MOD_EXPORT void _INIT_() {
   
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new RX888SourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RX888SourceModule*)instance;
}

MOD_EXPORT void _STOP_() {
    
}