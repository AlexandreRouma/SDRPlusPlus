#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <chrono>
#include <algorithm>
#include <fstream> // Added for file operations
#include <core.h>

SDRPP_MOD_INFO{
    /* Name:            */ "scanner",
    /* Description:     */ "Frequency scanner for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class ScannerModule : public ModuleManager::Instance {
public:
    ScannerModule(std::string name) {
        this->name = name;
        gui::menu.registerEntry(name, menuHandler, this, NULL);
        loadConfig();
    }

    ~ScannerModule() {
        saveConfig();
        gui::menu.removeEntry(name);
        stop();
    }

    void postInit() {}

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
        ScannerModule* _this = (ScannerModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;
        
        if (_this->running) { ImGui::BeginDisabled(); }
        ImGui::LeftLabel("Start");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputDouble("##start_freq_scanner", &_this->startFreq, 100.0, 100000.0, "%0.0f")) {
            _this->startFreq = round(_this->startFreq);
            _this->saveConfig();
        }
        ImGui::LeftLabel("Stop");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputDouble("##stop_freq_scanner", &_this->stopFreq, 100.0, 100000.0, "%0.0f")) {
            _this->stopFreq = round(_this->stopFreq);
            _this->saveConfig();
        }
        ImGui::LeftLabel("Interval");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputDouble("##interval_scanner", &_this->interval, 100.0, 100000.0, "%0.0f")) {
            _this->interval = round(_this->interval);
            _this->saveConfig();
        }
        ImGui::LeftLabel("Passband Ratio (%)");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputDouble("##pb_ratio_scanner", &_this->passbandRatio, 1.0, 10.0, "%0.0f")) {
            _this->passbandRatio = std::clamp<double>(round(_this->passbandRatio), 1.0, 100.0);
            _this->saveConfig();
        }
        ImGui::LeftLabel("Tuning Time (ms)");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt("##tuning_time_scanner", &_this->tuningTime, 100, 1000)) {
            _this->tuningTime = std::clamp<int>(_this->tuningTime, 100, 10000.0);
            _this->saveConfig();
        }
        ImGui::LeftLabel("Linger Time (ms)");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt("##linger_time_scanner", &_this->lingerTime, 100, 1000)) {
            _this->lingerTime = std::clamp<int>(_this->lingerTime, 100, 10000.0);
            _this->saveConfig();
        }
        if (_this->running) { ImGui::EndDisabled(); }

        ImGui::LeftLabel("Level");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat("##scanner_level", &_this->level, -150.0, 0.0)) {
            _this->saveConfig();
        }

        // Blacklist controls
        ImGui::Separator();
        ImGui::Text("Frequency Blacklist");
        
        // Add frequency to blacklist
        static double newBlacklistFreq = 0.0;
        ImGui::LeftLabel("Add Frequency (Hz)");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputDouble("##new_blacklist_freq", &newBlacklistFreq, 1000.0, 100000.0, "%0.0f")) {
            newBlacklistFreq = round(newBlacklistFreq);
        }
        if (ImGui::Button("Add to Blacklist##scanner_add_blacklist", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            if (newBlacklistFreq > 0) {
                _this->blacklistedFreqs.push_back(newBlacklistFreq);
                newBlacklistFreq = 0.0;
                _this->saveConfig();
            }
        }
        
        // Blacklist tolerance
        ImGui::LeftLabel("Blacklist Tolerance (Hz)");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputDouble("##blacklist_tolerance", &_this->blacklistTolerance, 100.0, 10000.0, "%0.0f")) {
            _this->blacklistTolerance = std::clamp<double>(round(_this->blacklistTolerance), 100.0, 100000.0);
            _this->saveConfig();
        }
        
        // List of blacklisted frequencies
        if (!_this->blacklistedFreqs.empty()) {
            ImGui::Text("Blacklisted Frequencies:");
            for (size_t i = 0; i < _this->blacklistedFreqs.size(); i++) {
                ImGui::SameLine();
                if (ImGui::Button(("Remove##scanner_remove_blacklist_" + std::to_string(i)).c_str())) {
                    _this->blacklistedFreqs.erase(_this->blacklistedFreqs.begin() + i);
                    _this->saveConfig();
                    break;
                }
                ImGui::SameLine();
                ImGui::Text("%.0f Hz", _this->blacklistedFreqs[i]);
            }
            
            if (ImGui::Button("Clear All Blacklisted##scanner_clear_blacklist", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                _this->blacklistedFreqs.clear();
                _this->saveConfig();
            }
        }

        ImGui::BeginTable(("scanner_bottom_btn_table" + _this->name).c_str(), 2);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button(("<<##scanner_back_" + _this->name).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            std::lock_guard<std::mutex> lck(_this->scanMtx);
            _this->reverseLock = true;
            _this->receiving = false;
            _this->scanUp = false;
        }
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button((">>##scanner_forw_" + _this->name).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            std::lock_guard<std::mutex> lck(_this->scanMtx);
            _this->reverseLock = true;
            _this->receiving = false;
            _this->scanUp = true;
        }
        ImGui::EndTable();

        if (!_this->running) {
            if (ImGui::Button("Start##scanner_start", ImVec2(menuWidth, 0))) {
                _this->start();
            }
            ImGui::Text("Status: Idle");
        }
        else {
            ImGui::BeginTable(("scanner_control_table" + _this->name).c_str(), 2);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button("Stop##scanner_start", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                _this->stop();
            }
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button("Reset##scanner_reset", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                _this->reset();
            }
            ImGui::EndTable();
            
            if (_this->receiving) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Receiving");
            }
            else if (_this->tuning) {
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "Status: Tuning");
            }
            else {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Scanning");
            }
        }
    }

    void start() {
        if (running) { return; }
        current = startFreq;
        running = true;
        workerThread = std::thread(&ScannerModule::worker, this);
    }

    void stop() {
        if (!running) { return; }
        running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    void reset() {
        std::lock_guard<std::mutex> lck(scanMtx);
        current = startFreq;
        receiving = false;
        tuning = false;
        reverseLock = false;
        flog::warn("Scanner: Reset to start frequency {:.0f} Hz", startFreq);
    }

    void saveConfig() {
        config.acquire();
        config.conf["startFreq"] = startFreq;
        config.conf["stopFreq"] = stopFreq;
        config.conf["interval"] = interval;
        config.conf["passbandRatio"] = passbandRatio;
        config.conf["tuningTime"] = tuningTime;
        config.conf["lingerTime"] = lingerTime;
        config.conf["level"] = level;
        config.conf["blacklistTolerance"] = blacklistTolerance;
        config.conf["blacklistedFreqs"] = blacklistedFreqs;
        config.release(true);
    }

    void loadConfig() {
        config.acquire();
        startFreq = config.conf.value("startFreq", 88000000.0);
        stopFreq = config.conf.value("stopFreq", 108000000.0);
        interval = config.conf.value("interval", 100000.0);
        passbandRatio = config.conf.value("passbandRatio", 10.0);
        tuningTime = config.conf.value("tuningTime", 250);
        lingerTime = config.conf.value("lingerTime", 1000.0);
        level = config.conf.value("level", -50.0);
        blacklistTolerance = config.conf.value("blacklistTolerance", 1000.0);
        if (config.conf.contains("blacklistedFreqs")) {
            blacklistedFreqs = config.conf["blacklistedFreqs"].get<std::vector<double>>();
        }
        config.release();
        
        // Ensure current frequency is within bounds
        if (current < startFreq || current > stopFreq) {
            current = startFreq;
        }
    }

    void worker() {
        // 10Hz scan loop
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            {
                std::lock_guard<std::mutex> lck(scanMtx);
                auto now = std::chrono::high_resolution_clock::now();

                // Enforce tuning
                if (gui::waterfall.selectedVFO.empty()) {
                    running = false;
                    return;
                }
                
                // Ensure current frequency is within bounds
                if (current < startFreq || current > stopFreq) {
                    flog::warn("Scanner: Current frequency {:.0f} Hz out of bounds, resetting to start", current);
                    current = startFreq;
                }
                
                tuner::normalTuning(gui::waterfall.selectedVFO, current);

                // Check if we are waiting for a tune
                if (tuning) {
                    flog::warn("Tuning");
                    if ((std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTuneTime)).count() > tuningTime) {
                        tuning = false;
                    }
                    continue;
                }

                // Get FFT data
                int dataWidth = 0;
                float* data = gui::waterfall.acquireLatestFFT(dataWidth);
                if (!data) { continue; }

                // Get gather waterfall data
                double wfCenter = gui::waterfall.getViewOffset() + gui::waterfall.getCenterFrequency();
                double wfWidth = gui::waterfall.getViewBandwidth();
                double wfStart = wfCenter - (wfWidth / 2.0);
                double wfEnd = wfCenter + (wfWidth / 2.0);

                // Gather VFO data
                double vfoWidth = sigpath::vfoManager.getBandwidth(gui::waterfall.selectedVFO);

                if (receiving) {
                    flog::warn("Receiving");
                
                    float maxLevel = getMaxLevel(data, current, vfoWidth, dataWidth, wfStart, wfWidth);
                    if (maxLevel >= level) {
                        lastSignalTime = now;
                    }
                    else if ((std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSignalTime)).count() > lingerTime) {
                        receiving = false;
                    }
                }
                else {
                    flog::warn("Seeking signal");
                    double bottomLimit = current;
                    double topLimit = current;
                    
                    // Search for a signal in scan direction
                    if (findSignal(scanUp, bottomLimit, topLimit, wfStart, wfEnd, wfWidth, vfoWidth, data, dataWidth)) {
                        gui::waterfall.releaseLatestFFT();
                        continue;
                    }
                    
                    // Search for signal in the inverse scan direction if direction isn't enforced
                    if (!reverseLock) {
                        if (findSignal(!scanUp, bottomLimit, topLimit, wfStart, wfEnd, wfWidth, vfoWidth, data, dataWidth)) {
                            gui::waterfall.releaseLatestFFT();
                            continue;
                        }
                    }
                    else { reverseLock = false; }
                    

                    // There is no signal on the visible spectrum, tune in scan direction and retry
                    if (scanUp) {
                        current = topLimit + interval;
                        // Ensure proper wrapping with bounds checking
                        while (current > stopFreq) {
                            current = startFreq + (current - stopFreq - interval);
                        }
                        if (current < startFreq) { current = startFreq; }
                    }
                    else {
                        current = bottomLimit - interval;
                        // Ensure proper wrapping with bounds checking
                        while (current < startFreq) {
                            current = stopFreq - (startFreq - current - interval);
                        }
                        if (current > stopFreq) { current = stopFreq; }
                    }
                    
                    // Add debug logging
                    flog::warn("Scanner: Tuned to {:.0f} Hz (range: {:.0f} - {:.0f})", current, startFreq, stopFreq);

                    // If the new current frequency is outside the visible bandwidth, wait for retune
                    if (current - (vfoWidth/2.0) < wfStart || current + (vfoWidth/2.0) > wfEnd) {
                        lastTuneTime = now;
                        tuning = true;
                    }
                }

                // Release FFT Data
                gui::waterfall.releaseLatestFFT();
            }
        }
    }

    bool findSignal(bool scanDir, double& bottomLimit, double& topLimit, double wfStart, double wfEnd, double wfWidth, double vfoWidth, float* data, int dataWidth) {
        bool found = false;
        double freq = current;
        int maxIterations = 1000; // Prevent infinite loops
        int iterations = 0;
        
        for (freq += scanDir ? interval : -interval;
            scanDir ? (freq <= stopFreq) : (freq >= startFreq);
            freq += scanDir ? interval : -interval) {
            
            iterations++;
            if (iterations > maxIterations) {
                flog::warn("Scanner: Max iterations reached, forcing frequency wrap");
                break;
            }

            // Check if signal is within bounds
            if (freq - (vfoWidth/2.0) < wfStart) { break; }
            if (freq + (vfoWidth/2.0) > wfEnd) { break; }

            // Check if frequency is blacklisted
            if (std::any_of(blacklistedFreqs.begin(), blacklistedFreqs.end(),
                            [freq, this](double blacklistedFreq) {
                                return std::abs(freq - blacklistedFreq) < blacklistTolerance;
                            })) {
                continue;
            }

            if (freq < bottomLimit) { bottomLimit = freq; }
            if (freq > topLimit) { topLimit = freq; }
            
            // Check signal level
            float maxLevel = getMaxLevel(data, freq, vfoWidth * (passbandRatio * 0.01f), dataWidth, wfStart, wfWidth);
            if (maxLevel >= level) {
                found = true;
                receiving = true;
                current = freq;
                break;
            }
        }
        return found;
    }

    float getMaxLevel(float* data, double freq, double width, int dataWidth, double wfStart, double wfWidth) {
        double low = freq - (width/2.0);
        double high = freq + (width/2.0);
        int lowId = std::clamp<int>((low - wfStart) * (double)dataWidth / wfWidth, 0, dataWidth - 1);
        int highId = std::clamp<int>((high - wfStart) * (double)dataWidth / wfWidth, 0, dataWidth - 1);
        float max = -INFINITY;
        for (int i = lowId; i <= highId; i++) {
            if (data[i] > max) { max = data[i]; }
        }
        return max;
    }

    std::string name;
    bool enabled = true;
    
    bool running = false;
    //std::string selectedVFO = "Radio";
    double startFreq = 88000000.0;
    double stopFreq = 108000000.0;
    double interval = 100000.0;
    double current = 88000000.0;
    double passbandRatio = 10.0;
    int tuningTime = 250;
    int lingerTime = 1000.0;
    float level = -50.0;
    bool receiving = true;
    bool tuning = false;
    bool scanUp = true;
    bool reverseLock = false;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastSignalTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTuneTime;
    std::thread workerThread;
    std::mutex scanMtx;
    
    // Blacklist functionality
    std::vector<double> blacklistedFreqs;
    double blacklistTolerance = 1000.0; // Tolerance in Hz for blacklisted frequencies
    

};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["startFreq"] = 88000000.0;
    def["stopFreq"] = 108000000.0;
    def["interval"] = 100000.0;
    def["passbandRatio"] = 10.0;
    def["tuningTime"] = 250;
    def["lingerTime"] = 1000.0;
    def["level"] = -50.0;
    def["blacklistTolerance"] = 1000.0;
    def["blacklistedFreqs"] = json::array();

    config.setPath(core::args["root"].s() + "/scanner_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new ScannerModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (ScannerModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}