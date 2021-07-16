#include <utils/networking.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <recorder_interface.h>


SDRPP_MOD_INFO {
    /* Name:            */ "rigctl_server",
    /* Description:     */ "My fancy new module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

class SigctlServerModule : public ModuleManager::Instance {
public:
    SigctlServerModule(std::string name) {
        this->name = name;
        
        strcpy(hostname, "localhost");

        gui::menu.registerEntry(name, menuHandler, this, NULL);
    }

    ~SigctlServerModule() {
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
        SigctlServerModule* _this = (SigctlServerModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        bool listening = (_this->listener && _this->listener->isListening());

        if (listening) { style::beginDisabled(); }
        ImGui::InputText("##testestssss", _this->hostname, 1023);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        ImGui::InputInt("##anothertest", &_this->port, 0, 0);
        if (listening) { style::endDisabled(); }

        int test = 0;

        ImGui::Text("Controlled VFO");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        ImGui::Combo("##anotesssss", &test, "Radio\0");

        ImGui::Text("Controlled Recorder");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        ImGui::Combo("##anotesssss2", &test, "Recorder\0");

        if (listening && ImGui::Button("Stop##lasttestamirite", ImVec2(menuWidth, 0))) {
            if (_this->client) { _this->client->close(); }
            _this->listener->close();
        }
        else if (!listening && ImGui::Button("Start##lasttestamirite", ImVec2(menuWidth, 0))) {
            try {
                _this->listener = net::listen(net::PROTO_TCP, _this->hostname, _this->port);
                _this->listener->acceptAsync(clientHandler, _this);
            }
            catch (std::exception e) {
                spdlog::error("Could not start rigctl server: {0}", e.what());
            }
        }

        ImGui::Text("Status:");
        ImGui::SameLine();
        if (_this->client && _this->client->isOpen()) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), "Connected");
        }
        else if (listening) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Listening");
        }
        else {
            ImGui::Text("Idle");
        }
    }

    static void clientHandler(net::Conn _client, void* ctx) {
        SigctlServerModule* _this = (SigctlServerModule*)ctx;
        spdlog::info("New client!");

        _this->client = std::move(_client);
        _this->client->readAsync(1024, _this->dataBuf, dataHandler, _this);
        _this->client->waitForEnd();
        _this->client->close();

        spdlog::info("Client disconnected!");

        _this->listener->acceptAsync(clientHandler, _this);
    }

    static void dataHandler(int count, uint8_t* data, void* ctx) {
        SigctlServerModule* _this = (SigctlServerModule*)ctx;

        for (int i = 0; i < count; i++) {
            if (data[i] == '\n') {
                _this->commandHandler(_this->command);
                _this->command.clear();
                continue;
            }
            _this->command += (char)data[i];
        }

        _this->client->readAsync(1024, _this->dataBuf, dataHandler, _this);
    }

    void commandHandler(std::string cmd) {
        std::string corr = "";
        std::vector<std::string> parts;
        bool lastWasSpace = false;
        std::string resp = "";

        // Split command into parts and remove excess spaces
        for (char c : cmd) {
            if (lastWasSpace && c == ' ') { continue; }
            else if (c == ' ') {
                parts.push_back(corr);
                corr.clear();
                lastWasSpace = true;
            }
            else {
                lastWasSpace = false;
                corr += c;
            }
        }
        if (!corr.empty()) {
            parts.push_back(corr);
        }

        // NOTE: THIS STUFF ISN'T THREADSAFE AND WILL LIKELY BREAK.

        // Execute commands
        if (parts.size() == 0) { return; }
        else if (parts[0] == "F") {
            // if number of arguments isn't correct, return error
            if (parts.size() != 2) {
                resp = "RPRT 1\n";
                client->write(resp.size(), (uint8_t*)resp.c_str());
                return;
            }

            // Parse frequency and assign it to the VFO
            long long freq = std::stoll(parts[1]);
            resp = "RPRT 0\n";
            client->write(resp.size(), (uint8_t*)resp.c_str());
            tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, freq);
        }
        else if (parts[0] == "f") {
            double freq = gui::waterfall.getCenterFrequency();

            // Get frequency of VFO if it exists
            if (sigpath::vfoManager.vfoExists(gui::waterfall.selectedVFO)) {
                freq += sigpath::vfoManager.getOffset(gui::waterfall.selectedVFO);
            }

            char buf[128];
            sprintf(buf, "%" PRIu64 "\n", (uint64_t)freq);
            client->write(strlen(buf), (uint8_t*)buf);
        }
        else if (parts[0] == "AOS") {
            // TODO: Start Recorder
            core::modComManager.callInterface("Recorder", RECORDER_IFACE_CMD_START, NULL, NULL);
            resp = "RPRT 0\n";
            client->write(resp.size(), (uint8_t*)resp.c_str());
        }
        else if (parts[0] == "LOS") {
            // TODO: Stop Recorder
             core::modComManager.callInterface("Recorder", RECORDER_IFACE_CMD_STOP, NULL, NULL);
             resp = "RPRT 0\n";
            client->write(resp.size(), (uint8_t*)resp.c_str());
        }
        else if (parts[0] == "q") {
            // Will close automatically
        }
        else {
            spdlog::error("Rigctl client sent invalid command: '{0}'", cmd);
            resp = "RPRT 1\n";
            client->write(resp.size(), (uint8_t*)resp.c_str());
            return;
        }
    }

    std::string name;
    bool enabled = true;

    char hostname[1024];
    int port = 4532;
    uint8_t dataBuf[1024];
    net::Listener listener;
    net::Conn client;

    std::string command = "";

};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SigctlServerModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (SigctlServerModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}