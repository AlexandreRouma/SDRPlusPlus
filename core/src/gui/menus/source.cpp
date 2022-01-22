#include <gui/menus/source.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>

namespace sourcemenu {
    int offsetMode = 0;
    int sourceId = 0;
    double customOffset = 0.0;
    double effectiveOffset = 0.0;
    int decimationPower = 0;
    bool iqCorrection = false;

    EventHandler<std::string> sourceRegisteredHandler;
    EventHandler<std::string> sourceUnregisterHandler;
    EventHandler<std::string> sourceUnregisteredHandler;

    std::vector<std::string> sourceNames;
    std::string sourceNamesTxt;
    std::string selectedSource;

    enum {
        OFFSET_MODE_NONE,
        OFFSET_MODE_CUSTOM,
        OFFSET_MODE_SPYVERTER,
        OFFSET_MODE_HAM_IT_UP,
        OFFSET_MODE_DK5AV_XB,
        OFFSET_MODE_KU_LNB_9750,
        OFFSET_MODE_KU_LNB_10700,
        _OFFSET_MODE_COUNT
    };

    const char* offsetModesTxt = "None\0"
                                 "Custom\0"
                                 "SpyVerter\0"
                                 "Ham-It-Up\0"
                                 "DK5AV X-Band\0"
                                 "Ku LNB (9750MHz)\0"
                                 "Ku LNB (10700MHz)\0";

    const char* decimationStages = "None\0"
                                   "2\0"
                                   "4\0"
                                   "8\0"
                                   "16\0"
                                   "32\0"
                                   "64\0";

    void updateOffset() {
        if (offsetMode == OFFSET_MODE_CUSTOM) { effectiveOffset = customOffset; }
        else if (offsetMode == OFFSET_MODE_SPYVERTER) {
            effectiveOffset = 120000000;
        } // 120MHz Up-conversion
        else if (offsetMode == OFFSET_MODE_HAM_IT_UP) {
            effectiveOffset = 125000000;
        } // 125MHz Up-conversion
        else if (offsetMode == OFFSET_MODE_DK5AV_XB) {
            effectiveOffset = -6800000000;
        } // 6.8GHz Down-conversion
        else if (offsetMode == OFFSET_MODE_KU_LNB_9750) {
            effectiveOffset = -9750000000;
        } // 9.750GHz Down-conversion
        else if (offsetMode == OFFSET_MODE_KU_LNB_10700) {
            effectiveOffset = -10700000000;
        } // 10.7GHz Down-conversion
        else {
            effectiveOffset = 0;
        }
        sigpath::sourceManager.setTuningOffset(effectiveOffset);
    }

    void refreshSources() {
        sourceNames = sigpath::sourceManager.getSourceNames();
        sourceNamesTxt.clear();
        for (auto name : sourceNames) {
            sourceNamesTxt += name;
            sourceNamesTxt += '\0';
        }
    }

    void selectSource(std::string name) {
        if (sourceNames.empty()) {
            selectedSource.clear();
            return;
        }
        auto it = std::find(sourceNames.begin(), sourceNames.end(), name);
        if (it == sourceNames.end()) {
            selectSource(sourceNames[0]);
            return;
        }
        sourceId = std::distance(sourceNames.begin(), it);
        selectedSource = sourceNames[sourceId];
        sigpath::sourceManager.selectSource(sourceNames[sourceId]);
    }

    void onSourceRegistered(std::string name, void* ctx) {
        refreshSources();

        if (selectedSource.empty()) {
            sourceId = 0;
            selectSource(sourceNames[0]);
            return;
        }

        sourceId = std::distance(sourceNames.begin(), std::find(sourceNames.begin(), sourceNames.end(), selectedSource));
    }

    void onSourceUnregister(std::string name, void* ctx) {
        if (name != selectedSource) { return; }

        // TODO: Stop everything
    }

    void onSourceUnregistered(std::string name, void* ctx) {
        refreshSources();

        if (sourceNames.empty()) {
            selectedSource = "";
            return;
        }

        if (name == selectedSource) {
            sourceId = std::clamp<int>(sourceId, 0, sourceNames.size() - 1);
            selectSource(sourceNames[sourceId]);
            return;
        }

        sourceId = std::distance(sourceNames.begin(), std::find(sourceNames.begin(), sourceNames.end(), selectedSource));
    }

    void init() {
        core::configManager.acquire();
        std::string selected = core::configManager.conf["source"];
        customOffset = core::configManager.conf["offset"];
        offsetMode = core::configManager.conf["offsetMode"];
        decimationPower = core::configManager.conf["decimationPower"];
        iqCorrection = core::configManager.conf["iqCorrection"];
        sigpath::signalPath.setIQCorrection(iqCorrection);
        updateOffset();

        refreshSources();
        selectSource(selected);
        sigpath::signalPath.setDecimation(decimationPower);

        sourceRegisteredHandler.handler = onSourceRegistered;
        sourceUnregisterHandler.handler = onSourceUnregister;
        sourceUnregisteredHandler.handler = onSourceUnregistered;
        sigpath::sourceManager.onSourceRegistered.bindHandler(&sourceRegisteredHandler);
        sigpath::sourceManager.onSourceUnregister.bindHandler(&sourceUnregisterHandler);
        sigpath::sourceManager.onSourceUnregistered.bindHandler(&sourceUnregisteredHandler);

        core::configManager.release();
    }

    void draw(void* ctx) {
        float itemWidth = ImGui::GetContentRegionAvailWidth();
        bool running = gui::mainWindow.sdrIsRunning();

        if (running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::Combo("##source", &sourceId, sourceNamesTxt.c_str())) {
            selectSource(sourceNames[sourceId]);
            core::configManager.acquire();
            core::configManager.conf["source"] = sourceNames[sourceId];
            core::configManager.release(true);
        }

        if (running) { style::endDisabled(); }

        sigpath::sourceManager.showSelectedMenu();

        if (ImGui::Checkbox("IQ Correction##_sdrpp_iq_corr", &iqCorrection)) {
            sigpath::signalPath.setIQCorrection(iqCorrection);
            core::configManager.acquire();
            core::configManager.conf["iqCorrection"] = iqCorrection;
            core::configManager.release(true);
        }

        ImGui::LeftLabel("Offset mode");
        ImGui::SetNextItemWidth(itemWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##_sdrpp_offset_mode", &offsetMode, offsetModesTxt)) {
            updateOffset();
            core::configManager.acquire();
            core::configManager.conf["offsetMode"] = offsetMode;
            core::configManager.release(true);
        }

        ImGui::LeftLabel("Offset");
        ImGui::SetNextItemWidth(itemWidth - ImGui::GetCursorPosX());
        if (offsetMode == OFFSET_MODE_CUSTOM) {
            if (ImGui::InputDouble("##freq_offset", &customOffset, 1.0, 100.0)) {
                updateOffset();
                core::configManager.acquire();
                core::configManager.conf["offset"] = customOffset;
                core::configManager.release(true);
            }
        }
        else {
            style::beginDisabled();
            ImGui::InputDouble("##freq_offset", &effectiveOffset, 1.0, 100.0);
            style::endDisabled();
        }

        if (running) { style::beginDisabled(); }
        ImGui::LeftLabel("Decimation");
        ImGui::SetNextItemWidth(itemWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##source_decim", &decimationPower, decimationStages)) {
            sigpath::signalPath.setDecimation(decimationPower);
            core::configManager.acquire();
            core::configManager.conf["decimationPower"] = decimationPower;
            core::configManager.release(true);
        }
        if (running) { style::endDisabled(); }
    }
}
