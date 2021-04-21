#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <discord_rpc.h>
#include <thread>

SDRPP_MOD_INFO {
    /* Name:            */ "discord-integration",
    /* Description:     */ "Discord Rich Presence module for SDR++",
    /* Author:          */ "Starman0620 & Ryzerth",
    /* Version:         */ 0, 0, 2,
    /* Max instances    */ 1
};

DiscordRichPresence presence;
uint64_t lastFreq;
char* freq = new char[24];

// Threading
int workerCounter = 0;
std::thread workerThread;
bool workerRunning;

class PresenceModule : public ModuleManager::Instance {
public:
    PresenceModule(std::string name) {
        this->name = name;

        workerRunning = true;
        workerThread = std::thread(&PresenceModule::worker, this);

        startPresence();
    }

    ~PresenceModule() {
        workerRunning = false;
        if (workerThread.joinable()) { workerThread.join(); }
    }

    void enable() {
        workerRunning = true;
        enabled = true;
    }

    void disable() {
        workerRunning = false;
        enabled = false;
        Discord_ClearPresence();
    }

    bool isEnabled() {
        return enabled;
    }

private:

    // Main thread
    void worker() {
        while (workerRunning) {
            workerCounter++;
            if(workerCounter >= 1000) {
                workerCounter = 0;
                updatePresence();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
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