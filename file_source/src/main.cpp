#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>

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
        spdlog::info("FileSourceModule '{0}': Instance created!", name);
    }

    ~FileSourceModule() {
        spdlog::info("FileSourceModule '{0}': Instance deleted!", name);
    }

private:
    static void menuSelected(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        spdlog::info("FileSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        spdlog::info("FileSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
        spdlog::info("FileSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        FileSourceModule* _this = (FileSourceModule*)ctx;
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

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
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