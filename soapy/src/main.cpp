#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>

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
    }

    ~SoapyModule() {
        spdlog::info("SoapyModule '{0}': Instance deleted!", name);
    }

private:
    static void menuSelected(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    
    static void menuHandler(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        ImGui::Text("Hi from %s!", _this->name.c_str());
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
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