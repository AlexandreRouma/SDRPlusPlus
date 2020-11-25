#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <iio.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

MOD_INFO {
    /* Name:        */ "plutosdr_source",
    /* Description: */ "PlutoSDR input module for SDR++",
    /* Author:      */ "Ryzerth",
    /* Version:     */ "0.1.0"
};

const char* gainModes[] = {
    "manual", "fast_attack", "slow_attack", "hybrid"
};

const char* gainModesTxt = "Manual\0Fast Attack\0Slow Attack\0Hybrid\0";

class PlutoSDRSourceModule {
public:
    PlutoSDRSourceModule(std::string name) {
        this->name = name;

        sampleRate = 4000000.0;	            

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("PlutoSDR", &handler);

        spdlog::info("PlutoSDRSourceModule '{0}': Instance created!", name);
    }

    ~PlutoSDRSourceModule() {
        spdlog::info("PlutoSDRSourceModule '{0}': Instance deleted!", name);
    }

private:
    static void menuSelected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("PlutoSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        spdlog::info("PlutoSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (_this->running) {
            return;
        }
        
        // TODO: INIT CONTEXT HERE
        _this->ctx = iio_create_context_from_uri("ip:192.168.2.1");
        if (_this->ctx == NULL) {
            spdlog::error("Could not open pluto");
            return;
        }
	    _this->phy = iio_context_find_device(_this->ctx, "ad9361-phy");
        if (_this->phy == NULL) {
            spdlog::error("Could not connect to pluto phy");
            iio_context_destroy(_this->ctx);
            return;
        }
        _this->dev = iio_context_find_device(_this->ctx, "cf-ad9361-lpc");
        if (_this->dev == NULL) {
            spdlog::error("Could not connect to pluto dev");
            iio_context_destroy(_this->ctx);
            return;
        }

        // Configure pluto
        iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "altvoltage0", true), "frequency", _this->freq); // Freq
	    iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false), "sampling_frequency", _this->sampleRate); // Sample rate
        iio_channel_attr_write(iio_device_find_channel(_this->phy, "voltage0", false), "gain_control_mode", "manual"); // manual gain
        iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false), "hardwaregain", _this->gain); // gain

        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        spdlog::info("PlutoSDRSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();

        // DESTROY CONTEXT HERE
        if (_this->ctx != NULL) {
            iio_context_destroy(_this->ctx);
            _this->ctx = NULL;
        }

        spdlog::info("PlutoSDRSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (_this->running) {
            // SET PLUTO FREQ HERE
            iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "altvoltage0", true), "frequency", _this->freq);
        }
        _this->freq = freq;
        spdlog::info("PlutoSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        char test[] = "192.168.2.1";
        ImGui::Text("IP");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        ImGui::InputText("", test, 16);
        
        // SELECT PLUTO HERE
        ImGui::Text("Gain Mode");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("", &_this->gainMode, gainModesTxt)) {

        }

        ImGui::Text("PGA Gain");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(CONCAT("##_gain_select_", _this->name), &_this->gain, 0, 76)) {
            if (_this->running) {
                // SET PLUTO GAIN HERE
                iio_channel_attr_write_longlong(iio_device_find_channel(_this->phy, "voltage0", false),"hardwaregain", _this->gain);
            }
        }
    }

    static void worker(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        int blockSize = _this->sampleRate / 200.0;

        struct iio_channel *rx0_i, *rx0_q;
        struct iio_buffer *rxbuf;
    
        rx0_i = iio_device_find_channel(_this->dev, "voltage0", 0);
        rx0_q = iio_device_find_channel(_this->dev, "voltage1", 0);
    
        iio_channel_enable(rx0_i);
        iio_channel_enable(rx0_q);
    
        rxbuf = iio_device_create_buffer(_this->dev, blockSize, false);
        if (!rxbuf) {
            spdlog::error("Could not create RX buffer");
            return;
        }

        while (true) {
            // Read samples here
            // TODO: RECEIVE HERE
            iio_buffer_refill(rxbuf);

            int16_t* buf = (int16_t*)iio_buffer_first(rxbuf, rx0_i);

            if (_this->stream.aquire() < 0) { break; }
            for (int i = 0; i < blockSize; i++) {
                _this->stream.data[i].q = (float)buf[i * 2] / 32768.0f;
                _this->stream.data[i].i = (float)buf[(i * 2) + 1] / 32768.0f;
            }
            _this->stream.write(blockSize);
        }

        iio_buffer_destroy(rxbuf);
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    struct iio_context *ctx = NULL;
	struct iio_device *phy = NULL;
    struct iio_device *dev = NULL;
    bool running = false;
    double freq;
    char ip[1024] = "localhost";
    int port = 1234;
    int gainMode = 0;
    float gain = 0;
};

MOD_EXPORT void _INIT_() {
   // Do your one time init here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new PlutoSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (PlutoSDRSourceModule*)instance;
}

MOD_EXPORT void _STOP_() {
    // Do your one shutdown here
}