#include <imgui.h>
#include <module.h>
#include <gui/gui.h>

SDRPP_MOD_INFO {
    /* Name:            */ "demo",
    /* Description:     */ "My fancy new module",
    /* Author:          */ "author1;author2,author3,etc...",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

class DemoModule : public ModuleManager::Instance {
public:
    DemoModule(std::string name) {
        this->name = name;
        gui::menu.registerEntry(name, menuHandler, this, NULL);
    }

    ~DemoModule() {
        gui::menu.removeEntry(name);
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
        DemoModule* _this = (DemoModule*)ctx;
        ImGui::Text("Hello SDR++, my name is %s", _this->name.c_str());
    }

    std::string name;
    bool enabled = true;

};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new DemoModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (DemoModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}