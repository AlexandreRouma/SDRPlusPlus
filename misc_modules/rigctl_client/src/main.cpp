#include <utils/proto/rigctl.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <recorder_interface.h>
#include <meteor_demodulator_interface.h>
#include <config.h>
#include <cctype>
#include <radio_interface.h>
#include <utils/optionlist.h>
#include <atomic>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "rigctl_client",
    /* Description:     */ "Client for the RigCTL protocol",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ 1
};

ConfigManager config;

enum Mode {
    MODE_PANADAPTER,
    MODE_MIRROR
};

enum Priority {
    PRIOR_SDR,
    PRIOR_RIGCTL
};

const std::map<int, net::rigctl::Mode> RADIO_TO_RIGCTL = {
    { RADIO_IFACE_MODE_NFM, net::rigctl::MODE_FM  },
    { RADIO_IFACE_MODE_WFM, net::rigctl::MODE_WFM },
    { RADIO_IFACE_MODE_AM , net::rigctl::MODE_AM  },
    { RADIO_IFACE_MODE_DSB, net::rigctl::MODE_DSB },
    { RADIO_IFACE_MODE_USB, net::rigctl::MODE_USB },
    { RADIO_IFACE_MODE_CW , net::rigctl::MODE_CW  },
    { RADIO_IFACE_MODE_LSB, net::rigctl::MODE_LSB }
};

class RigctlClientModule : public ModuleManager::Instance {
public:
    RigctlClientModule(std::string name) {
        this->name = name;

        // Define the operation modes
        modes.define("panadapter", "Pandapter", MODE_PANADAPTER);
        modes.define("mirror", "Mirror", MODE_MIRROR);

        // Define the priority modes
        priorities.define("sdr", "SDR", PRIOR_SDR);
        priorities.define("rigctl", "RigCTL", PRIOR_RIGCTL);

        // Load default
        strcpy(host, "127.0.0.1");

        // Load config
        config.acquire();
        if (config.conf[name].contains("host")) {
            std::string h = config.conf[name]["host"];
            strcpy(host, h.c_str());
        }
        if (config.conf[name].contains("port")) {
            port = config.conf[name]["port"];
            port = std::clamp<int>(port, 1, 65535);
        }
        if (config.conf[name].contains("mode")) {
            std::string modeStr = config.conf[name]["mode"];
            if (modes.keyExists(modeStr)) { modeId = modes.keyId(modeStr); }
        }
        if (config.conf[name].contains("priority")) {
            std::string priorityStr = config.conf[name]["priority"];
            if (priorities.keyExists(priorityStr)) { priorityId = modes.keyId(priorityStr); }
        }
        if (config.conf[name].contains("interval")) {
            interval = config.conf[name]["interval"];
            interval = std::clamp<int>(interval, 100, 1000);
        }
        if (config.conf[name].contains("ifFreq")) {
            ifFreq = config.conf[name]["ifFreq"];
        }
        if (config.conf[name].contains("vfo")) {
            selectedVFO = config.conf[name]["vfo"];
        }
        config.release();

        // Refresh VFOs
        refreshVFOs();

        gui::menu.registerEntry(name, menuHandler, this, NULL);
    }

    ~RigctlClientModule() {
        stop();
        gui::menu.removeEntry(name);
    }

    void postInit() {
        
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

    void start() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (running) { return; }

        // Connect to rigctl server
        try {
            client = net::rigctl::connect(host, port);
        }
        catch (const std::exception& e) {
            flog::error("Could not connect: {}", e.what());
            return;
        }

        if (mode == MODE_PANADAPTER) {
            // Switch source to panadapter mode
            sigpath::sourceManager.setPanadapterIF(ifFreq);
            sigpath::sourceManager.setTuningMode(SourceManager::TuningMode::PANADAPTER);
        }

        // Start the worker thread
        run = true;
        if (mode == MODE_PANADAPTER) {
            workerThread = std::thread(&RigctlClientModule::panadapterWorker, this);
        }
        else if (mode == MODE_MIRROR) {
            workerThread = std::thread(&RigctlClientModule::mirrorWorker, this);
        }

        running = true;
    }

    void stop() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (!running) { return; }

        // Stop the worker thread
        run = false;
        if (workerThread.joinable()) { workerThread.join(); }

        if (mode == MODE_PANADAPTER) {
            // Switch source back to normal mode
            sigpath::sourceManager.onRetune.unbindHandler(&_retuneHandler);
            sigpath::sourceManager.setTuningMode(SourceManager::TuningMode::NORMAL);
        }

        // Disconnect from rigctl server
        client->close();

        running = false;
    }

private:
    static void menuHandler(void* ctx) {
        RigctlClientModule* _this = (RigctlClientModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (_this->running) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_rigctl_cli_host_", _this->name), _this->host, 1023)) {
            config.acquire();
            config.conf[_this->name]["host"] = std::string(_this->host);
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_rigctl_cli_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf[_this->name]["port"] = _this->port;
            config.release(true);
        }

        ImGui::LeftLabel("Mode");
        ImGui::FillWidth();
        if (ImGui::Combo(CONCAT("##_rigctl_cli_mode_", _this->name), &_this->modeId, _this->modes.txt)) {
            _this->mode = _this->modes[_this->modeId];
            config.acquire();
            config.conf[_this->name]["mode"] = _this->modes.key(_this->modeId);
            config.release(true);
        }

        ImGui::LeftLabel("Priority");
        ImGui::FillWidth();
        if (ImGui::Combo(CONCAT("##_rigctl_cli_priority_", _this->name), &_this->priorityId, _this->priorities.txt)) {
            _this->priority = _this->priorities[_this->priorityId];
            config.acquire();
            config.conf[_this->name]["priority"] = _this->priorities.key(_this->priorityId);
            config.release(true);
        }

        ImGui::LeftLabel("Interval");
        ImGui::FillWidth();
        if (ImGui::InputInt(CONCAT("##_rigctl_cli_interval_", _this->name), &_this->interval, 10, 100)) {
            _this->interval = std::clamp<int>(_this->interval, 100, 1000);
            config.acquire();
            config.conf[_this->name]["interval"] = _this->interval;
            config.release(true);
        }

        if (_this->mode == MODE_PANADAPTER) {
            ImGui::LeftLabel("IF Frequency");
            ImGui::FillWidth();
            if (ImGui::InputDouble(CONCAT("##_rigctl_if_freq_", _this->name), &_this->ifFreq, 100.0, 100000.0, "%.0f")) {
                if (_this->running) {
                    sigpath::sourceManager.setPanadapterIF(_this->ifFreq);
                }
                config.acquire();
                config.conf[_this->name]["ifFreq"] = _this->ifFreq;
                config.release(true);
            }
        }
        else if (_this->mode == MODE_MIRROR) {
            ImGui::LeftLabel("Controlled VFO");
            ImGui::FillWidth();
            if (ImGui::Combo(CONCAT("##_rigctl_cli_vfo_", _this->name), &_this->vfoId, _this->vfos.txt)) {
                _this->selectedVFO = _this->vfos[_this->vfoId];
                config.acquire();
                config.conf[_this->name]["vfo"] = _this->vfos.key(_this->vfoId);
                config.release(true);
            }

            ImGui::Checkbox(CONCAT("Sync Frequency##_rigctl_sync_freq_", _this->name), &_this->syncFrequency);
            if (_this->vfoIsRadio) {
                ImGui::Checkbox(CONCAT("Sync Mode##_rigctl_sync_freq_", _this->name), &_this->syncMode);
            }
            else {
                bool dummy = false;
                if (!_this->running) { style::beginDisabled(); }
                ImGui::Checkbox(CONCAT("Sync Mode##_rigctl_sync_freq_", _this->name), &dummy);
                if (!_this->running) { style::endDisabled(); }
            }
        }

        if (_this->running) { style::endDisabled(); }
        
        ImGui::FillWidth();
        if (_this->running && ImGui::Button(CONCAT("Stop##_rigctl_cli_stop_", _this->name), ImVec2(menuWidth, 0))) {
            _this->stop();
        }
        else if (!_this->running && ImGui::Button(CONCAT("Start##_rigctl_cli_stop_", _this->name), ImVec2(menuWidth, 0))) {
            _this->start();
        }

        ImGui::TextUnformatted("Status:");
        ImGui::SameLine();
        if (_this->client && _this->client->isOpen() && _this->running) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), "Connected");
        }
        else if (_this->client && _this->running) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Disconnected");
        }
        else {
            ImGui::TextUnformatted("Idle");
        }
    }

    void selectVFO(const std::string& vfoName) {
        // If no vfo is available, deselect
        if (vfos.empty()) {
            selectedVFO.clear();
            vfoIsRadio = false;
            return;
        }

        // If a vfo with that name isn't found, select the first VFO in the list
        if (!vfos.keyExists(vfoName)) {
            selectVFO(vfos.key(0));
            return;
        }

        // Check if the VFO is from a radio module
        vfoIsRadio = (core::moduleManager.getInstanceModuleName(vfoName) == "radio");

        // Update the selected VFO
        selectedVFO = vfoName;
        vfoId = vfos.keyId(vfoName);
    }

    void refreshVFOs() {
        // Clear the list
        vfos.clear();

        // Define using the VFO list from the waterfall
        for (auto const& [_name, vfo] : gui::waterfall.vfos) {
            vfos.define(_name, _name, _name);
        }
        
        // Reselect the current VFO
        selectVFO(selectedVFO);
    }

    void panadapterWorker() {
        int64_t lastRigctlFreq = -1;
        int64_t lastCenterFreq = -1;
        int64_t rigctlFreq;
        int64_t centerFreq;

        while (run) {
            // Query the current modes
            try {
                // Get the current rigctl frequency
                rigctlFreq = (int64_t)client->getFreq();

                // Get the current center frequency
                centerFreq = (int64_t)gui::waterfall.getCenterFrequency();
            }
            catch (const std::exception& e) {
                flog::error("Error while getting frequencies: {}", e.what());
            }

            // Update frequencies depending on the priority
            if (priority == PRIOR_SDR) {
                
            }
            else if (priority == PRIOR_RIGCTL) {

            }

            // Save frequencies
            lastRigctlFreq = rigctlFreq;
            lastCenterFreq = centerFreq;

            // Wait for the time interval
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }

    void mirrorWorker() {
        int64_t lastRigctlFreq = -1;
        int64_t lastFreq = -1;
        int64_t rigctlFreq;
        int64_t freq;
        int lastRigctlMode = -1;
        int lastVFOMode = -1;
        int rigctlMode;
        int vfoMode;

        while (run) {
            // Query the current modes
            try {
                // Get the current rigctl frequency
                rigctlFreq = (int64_t)client->getFreq();

                // Get the rigctl and VFO modes
                if (selectedVFO.empty()) {
                    // Get the VFO frequency
                    // TODO

                    // Get the mode if needed
                    if (syncMode && vfoIsRadio) {
                        // Get the current rigctl mode
                        // rigctlMode = client->getMode();

                        // Get the current VFO mode
                        core::modComManager.callInterface(selectedVFO, RADIO_IFACE_CMD_GET_MODE, NULL, &vfoMode);
                    }
                }
                else {
                    freq = (int64_t)gui::waterfall.getCenterFrequency();
                }
            }
            catch (const std::exception& e) {
                flog::error("Error while getting frequencies: {}", e.what());
            }

            // Update frequencies depending on the priority
            if (priority == PRIOR_SDR) {
                
            }
            else if (priority == PRIOR_RIGCTL) {

            }

            // Save modes and frequencies
            lastRigctlFreq = rigctlFreq;
            lastFreq = freq;
            lastRigctlMode = rigctlMode;
            lastVFOMode = vfoMode;

            // Wait for the time interval
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    std::recursive_mutex mtx;

    char host[1024];
    int port = 4532;
    std::shared_ptr<net::rigctl::Client> client;

    Mode mode = MODE_PANADAPTER;
    int modeId = 0;

    Priority priority = PRIOR_SDR;
    int priorityId = 0;

    int interval = 100;

    double ifFreq = 8830000.0;

    bool syncFrequency = true;
    bool syncMode = true;
    bool vfoIsRadio = false;

    EventHandler<double> _retuneHandler;

    OptionList<std::string, Mode> modes;
    OptionList<std::string, Priority> priorities;

    std::string selectedVFO = "";
    int vfoId = 0;
    OptionList<std::string, std::string> vfos;

    std::atomic_bool run;
    std::thread workerThread;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/rigctl_client_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RigctlClientModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RigctlClientModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
