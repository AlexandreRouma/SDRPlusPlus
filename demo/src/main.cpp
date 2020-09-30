#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

MOD_INFO {
    /* Name:        */ "demo",
    /* Description: */ "Demo module for SDR++",
    /* Author:      */ "Ryzerth",
    /* Version:     */ "0.1.0"
};

class DemoModule {
public:
    DemoModule(std::string name) {
        this->name = name;
        gui::menu.registerEntry(name, menuHandler, this);
        spdlog::info("DemoModule '{0}': Instance created!", name);
    }

    ~DemoModule() {
        spdlog::info("DemoModule '{0}': Instance deleted!", name);
    }

private:
    static void menuHandler(void* ctx) {
        DemoModule* _this = (DemoModule*)ctx;
        ImGui::Text("Hi from %s!", _this->name.c_str());
    }

    std::string name;

};

MOD_EXPORT void _INIT_() {
   // Do your one time init here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new DemoModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (DemoModule*)instance;
}

MOD_EXPORT void _STOP_() {
    // Do your one shutdown here
}