#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <cmath>
#include <discord_rpc.h>

SDRPP_MOD_INFO {
    /* Name:            */ "discord-integration",
    /* Description:     */ "Discord Rich Presence module for SDR++",
    /* Author:          */ "Starman0620",
    /* Version:         */ 0, 0, 1,
    /* Max instances    */ 1
};

void ready(const DiscordUser *request);
static DiscordRichPresence presence;
static uint64_t lastFreq;
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
        // For now this is empty, it's just left in so the toggle checkbox remains.
        ImGui::BeginGroup();
        ImGui::EndGroup();
        
        if (!_this->enabled) { style::endDisabled(); }
    }

    static void updatePresence() {
        if (gui::freqSelect.frequency != lastFreq) {
            presence.details = "Listening"; // This really doesn't need to be re-set each time but it'll remain since it will be used once proper status can be displayed
            sprintf(freq, "%.2fMHz", (float)gui::freqSelect.frequency/1000000);
            presence.state = freq;
            Discord_UpdatePresence(&presence);
            lastFreq = gui::freqSelect.frequency;
        }
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