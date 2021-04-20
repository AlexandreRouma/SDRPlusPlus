#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>

SDRPP_MOD_INFO {
    /* Name:            */ "discord",
    /* Description:     */ "Discord Rich Presence module for SDR++",
    /* Author:          */ "Starman0620",
    /* Version:         */ 0, 0, 1,
    /* Max instances    */ -1
};

class PresenceModule : public ModuleManager::Instance {
public:
    PresenceModule(std::string name) {
        this->name = name;
        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~PresenceModule() {

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
        PresenceModule* _this = (PresenceModule*)ctx;
        if (!_this->enabled) { style::beginDisabled(); }

        float menuWidth = ImGui::GetContentRegionAvailWidth();

        // GUI
        ImGui::BeginGroup();
        ImGui::Text("Connecting to Discord...");
        ImGui::EndGroup();
        
        if (!_this->enabled) { style::endDisabled(); }
    }

    std::string name;
    bool enabled = true;
};

MOD_EXPORT void _INIT_() {

}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new PresenceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (PresenceModule*)instance;
}

MOD_EXPORT void _END_() {

}