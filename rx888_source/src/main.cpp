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

#define ADC_RATE        128000000
#define XFER_TIMEOUT    5000

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

        decimation = 5;
        sampleRate = (ADC_RATE >> (decimation - 1));
        freq = sampleRate / 2;
        

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

        si5351aSetFrequency(ADC_RATE, 0);
        fx3Control(GPIOFX3, Bgpio);

        running = true;
        usbThread = std::thread(_usbWorker, this);
        workerThread = std::thread(_worker, this);
    }
    
    static void stop(void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        _this->running = false;
        _this->realStream.stopWriter();
        _this->realStream.stopReader();
        _this->stream.stopWriter();
        _this->usbThread.join();
        _this->workerThread.join();
        _this->realStream.clearWriteStop();
        _this->realStream.clearReadStop();
        _this->stream.clearWriteStop();
        spdlog::info("RX888SourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        _this->freq = freq;
        
        spdlog::info("RX888SourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    
    static void menuHandler(void* ctx) {
        RX888SourceModule* _this = (RX888SourceModule*)ctx;
        if (ImGui::InputInt("Decimation", &_this->decimation)) {
            _this->sampleRate = (ADC_RATE >> (_this->decimation - 1));
            core::setInputSampleRate(_this->sampleRate);
        }
    }

    static void _usbWorker(RX888SourceModule* _this) {
        // Calculate hardware block siz
        int realBlockSize = ADC_RATE / 200;
        int i;
        for (i = 1; i < realBlockSize; i = (i << 1));
        realBlockSize = (i >> 1);
        int realDataSize = realBlockSize * sizeof(int16_t);

        int16_t* buffer = new int16_t[realBlockSize];

        spdlog::info("Real block size: {0}", realBlockSize);

        // Initialize transfer
        long pktSize = EndPt->MaxPktSize;
        EndPt->SetXferSize(realDataSize);
        long ppx = realDataSize / pktSize;
        OVERLAPPED inOvLap;
        inOvLap.hEvent = CreateEvent(NULL, false, false, NULL);
        
        auto context = EndPt->BeginDataXfer((PUCHAR)buffer, realDataSize, &inOvLap);

        // Check weather the transfer begin was successful
        if (EndPt->NtStatus || EndPt->UsbdStatus) {
            spdlog::error("Xfer request rejected. 1 STATUS = {0} {1}", EndPt->NtStatus, EndPt->UsbdStatus);
            delete[] buffer;
            return;
        }

        // Start the USB chip
        fx3Control(STARTFX3);

        // Data loop
        while (_this->running) {
            // Wait for the transfer to begin and about if timeout
            LONG rLen = realDataSize;
            if (!EndPt->WaitForXfer(&inOvLap, XFER_TIMEOUT)) {
                spdlog::error("Transfer aborted");
                EndPt->Abort();
                if (EndPt->LastError == ERROR_IO_PENDING) {
                    WaitForSingleObject(inOvLap.hEvent, XFER_TIMEOUT);
                }
                break;
            }

            // Check if the incomming data is bulk I/Q and end transfer
            if (EndPt->Attributes == 2) {
                if (EndPt->FinishDataXfer((PUCHAR)buffer, rLen, &inOvLap, context)) {
                    if (_this->realStream.aquire() < 0) { break; }
                    memcpy(_this->realStream.data, buffer, rLen);
                    _this->realStream.write(rLen / 2);
                }
            }

            // Start next transfer
            context = EndPt->BeginDataXfer((PUCHAR)buffer, realBlockSize * 2, &inOvLap);
        }

        delete[] buffer;

        // Stop FX3 chip
        fx3Control(STOPFX3);
    }

    static void _worker(RX888SourceModule* _this) {
        dsp::complex_t* iqbuffer = new dsp::complex_t[STREAM_BUFFER_SIZE];
        lv_32fc_t phase = lv_cmake(1.0f, 0.0f);

        // Read from real stream
        int count = _this->realStream.read();
        int blockSize = count;

        while (count >= 0) {
            for (int i = 0; i < count; i++) {
                iqbuffer[i].q = (float)_this->realStream.data[i] / 32768.0f;
            }
            _this->realStream.flush();

            // Calculate the traslation frequency based on the tuning
            float delta = -(_this->freq / (float)ADC_RATE) * 2.0f * FL_M_PI;
            lv_32fc_t phaseDelta = lv_cmake(std::cos(delta), std::sin(delta));

            // Apply translation
            if (_this->stream.aquire() < 0) { break; }
            volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)_this->stream.data, (lv_32fc_t*)iqbuffer, phaseDelta, &phase, count);

            // Decimate
            blockSize = count;
            for (int d = 0; d < (_this->decimation - 1); d++) {
                blockSize = (blockSize >> 1);
                for (int i = 0; i < blockSize; i++) {
                    _this->stream.data[i].i = (_this->stream.data[i*2].i + _this->stream.data[(i*2)+1].i) * 0.5f;
                    _this->stream.data[i].q = (_this->stream.data[i*2].q + _this->stream.data[(i*2)+1].q) * 0.5f;
                }
            }

            // Write to output stream
            _this->stream.write(blockSize);

            // Read from real stream
            count = _this->realStream.read();
        }

        // Stop FX3 chip
        fx3Control(STOPFX3);

        // Deallocate buffers
        delete[] iqbuffer;
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    dsp::stream<int16_t> realStream;
    SourceManager::SourceHandler handler;
    std::thread usbThread;
    std::thread workerThread;
    double freq;
    double sampleRate;
    int decimation;
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