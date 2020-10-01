#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Logger.hpp>
#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

MOD_INFO {
    /* Name:        */ "soapy",
    /* Description: */ "SoapySDR input module for SDR++",
    /* Author:      */ "Ryzerth",
    /* Version:     */ "0.1.0"
};

class SoapyModule {
public:
    SoapyModule(std::string name) {
        this->name = name;

        //TODO: Make module tune on source select change (in sdrpp_core)

        devList = SoapySDR::Device::enumerate();
        txtDevList = "";
        for (auto& dev : devList) {
            txtDevList += dev["label"];
            txtDevList += '\0';
        }

        stream.init(100);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("SoapySDR", &handler);
        spdlog::info("SoapyModule '{0}': Instance created!", name);

        // Select default device
        selectDevice(devList[0]["label"]);
    }

    ~SoapyModule() {
        spdlog::info("SoapyModule '{0}': Instance deleted!", name);
    }

private:
    void selectSampleRate(double samplerate) {
        if (sampleRates.size() == 0) {
            devId = -1;
            return;
        }
        bool found = false;
        int i = 0;
        for (auto& sr : sampleRates) {
            if (sr == samplerate) {
                srId = i;
                sampleRate = sr;
                found = true;
                break;
            }
            i++;
        }
        if (!found) {
            // Select default sample rate
            selectSampleRate(sampleRates[0]);
        }
    }

    void selectDevice(std::string name) {
        if (devList.size() == 0) {
            devId = -1;
            return;
        }
        bool found = false;
        int i = 0;
        for (auto& args : devList) {
            if (args["label"] == name) {
                devArgs = args;
                devId = i;
                found = true;
                break;
            }
            i++;
        }
        if (!found) {
            // If device was not found, select default device instead
            selectDevice(devList[0]["label"]);
            return;
        }

        SoapySDR::Device* dev = SoapySDR::Device::make(devArgs);
        sampleRates = dev->listSampleRates(SOAPY_SDR_RX, 0);
        txtSrList = "";
        for (double sr : sampleRates) {
            txtSrList += std::to_string((int)sr);
            txtSrList += '\0';
        }
        SoapySDR::Device::unmake(dev);
        selectSampleRate(sampleRates[0]);

    }

    static void menuSelected(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Menu Select!", _this->name);
        if (_this->devList.size() == 0) {
            return;
        }
        core::setInputSampleRate(_this->sampleRate);
    }

    static void menuDeselected(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        _this->dev = SoapySDR::Device::make(_this->devArgs);
        _this->dev->setSampleRate(SOAPY_SDR_RX, 0, _this->sampleRate);
        _this->devStream = _this->dev->setupStream(SOAPY_SDR_RX, "CF32");
        _this->dev->activateStream(_this->devStream);
        _this->running = true;
        _this->workerThread = std::thread(_worker, _this);
        spdlog::info("SoapyModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        _this->running = false;
        spdlog::info("SoapyModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            _this->dev->setFrequency(SOAPY_SDR_RX, 0, freq);
        }
        spdlog::info("SoapyModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    
    static void menuHandler(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        
        // If no device is available, do not attempt to display menu
        if (_this->devId < 0) {
            return;
        }

        float menuWidth = ImGui::GetContentRegionAvailWidth();
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_dev_select_", _this->name), &_this->devId, _this->txtDevList.c_str())) {
            _this->selectDevice(_this->devList[_this->devId]["label"]);
        }
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_sr_select_", _this->name), &_this->srId, _this->txtSrList.c_str())) {
            _this->selectSampleRate(_this->sampleRates[_this->srId]);
            core::setInputSampleRate(_this->sampleRate);
        }
    }

    static void _worker(SoapyModule* _this) {
        spdlog::info("SOAPY: WORKER STARTED {0}", _this->sampleRate);
        int blockSize = _this->sampleRate / 200.0f;
        dsp::complex_t* buf = new dsp::complex_t[blockSize];
        int flags = 0;
        long long timeMs = 0;
        
        while (_this->running) {
            int res = _this->dev->readStream(_this->devStream, (void**)&buf, blockSize, flags, timeMs);
            if (res < 1) {
                continue;
            } 
            _this->stream.write(buf, res);
        }
        delete[] buf;
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SoapySDR::Stream* devStream;
    SourceManager::SourceHandler handler;
    SoapySDR::KwargsList devList;
    SoapySDR::Kwargs devArgs;
    SoapySDR::Device* dev;
    std::string txtDevList;
    std::string txtSrList;
    std::thread workerThread;
    int devId = -1;
    double freq;
    double sampleRate;
    bool running = false;
    std::vector<double> sampleRates;
    int srId = -1;
};

MOD_EXPORT void _INIT_() {
   // Do your one time init here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new SoapyModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (SoapyModule*)instance;
}

MOD_EXPORT void _STOP_() {
    // Do your one shutdown here
}