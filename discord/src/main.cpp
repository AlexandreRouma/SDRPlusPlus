#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <cmath>
#include <discord_rpc.h>

SDRPP_MOD_INFO {
    /* Name:            */ "discord",
    /* Description:     */ "Discord Rich Presence module for SDR++",
    /* Author:          */ "Starman0620",
    /* Version:         */ 0, 0, 1,
    /* Max instances    */ -1
};

void ready(const DiscordUser *request);
static DiscordRichPresence presence;
static time_t lastUpdate = time(0);
static char* freq = new char[24];

class PresenceModule : public ModuleManager::Instance {
public:
    PresenceModule(std::string name) {
        this->name = name;
        gui::menu.registerEntry(name, menuHandler, this, this);

        startPresence();
    }

    ~PresenceModule() {

    }

    void enable() {
        startPresence();
        enabled = true;
    }

    void disable() {
        Discord_ClearPresence();
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
        ImGui::Text("SDR++ Rich Presence by Starman0620");
        ImGui::EndGroup();

        // Very basic method of implenting a 10s timer
        if (time(0) - lastUpdate > 10) {
            updatePresence();
            lastUpdate = time(0);
        }
        
        if (!_this->enabled) { style::endDisabled(); }
    }

    static void updatePresence() {
       presence.details = "Listening";
       sprintf(freq, "%.2fMHz", gui::waterfall.getCenterFrequency()/1000000, 3);
       presence.state = freq;
       Discord_UpdatePresence(&presence);
    }

    static void startPresence() {
        // Discord initialization
        DiscordEventHandlers handlers;
        memset(&handlers, 0, sizeof(handlers));
        memset(&presence, 0, sizeof(presence));
        Discord_Initialize("833485588954742864", &handlers, 1, "");

        // Set the first presence
        presence.details = "Initializing rich presence...";
        presence.startTimestamp = time(0);
        presence.largeImageKey = "image_large";
        Discord_UpdatePresence(&presence);
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