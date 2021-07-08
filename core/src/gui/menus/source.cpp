#include <gui/menus/source.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>

namespace sourecmenu {
    int offsetMode = 0;
    int sourceId = 0;
    double customOffset = 0.0;
    double effectiveOffset = 0.0;

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

    void updateOffset() {
        if (offsetMode == OFFSET_MODE_CUSTOM) {         effectiveOffset = customOffset; }
        else if (offsetMode == OFFSET_MODE_SPYVERTER) { effectiveOffset = 120000000; }          // 120MHz Up-conversion
        else if (offsetMode == OFFSET_MODE_HAM_IT_UP) { effectiveOffset = 125000000; }          // 125MHz Up-conversion
        else if (offsetMode == OFFSET_MODE_DK5AV_XB) {  effectiveOffset = -6800000000; }         // 6.8GHz Down-conversion
        else if (offsetMode == OFFSET_MODE_KU_LNB_9750) { effectiveOffset = -9750000000; }       // 9.750GHz Down-conversion
        else if (offsetMode == OFFSET_MODE_KU_LNB_10700) {  effectiveOffset = -10700000000; }    // 10.7GHz Down-conversion
        else { effectiveOffset = 0; }
        sigpath::sourceManager.setTuningOffset(effectiveOffset);
    }

    void init() {
        core::configManager.aquire();
        std::string name = core::configManager.conf["source"];
        auto it = std::find(sigpath::sourceManager.sourceNames.begin(), sigpath::sourceManager.sourceNames.end(), name);
        if (it != sigpath::sourceManager.sourceNames.end()) {
            sigpath::sourceManager.selectSource(name);
            sourceId = std::distance(sigpath::sourceManager.sourceNames.begin(), it);
        }
        else if (sigpath::sourceManager.sourceNames.size() > 0) {
            sigpath::sourceManager.selectSource(sigpath::sourceManager.sourceNames[0]);
        }
        else {
            spdlog::warn("No source available...");
        }
        customOffset = core::configManager.conf["offset"];
        offsetMode = core::configManager.conf["offsetMode"];
        updateOffset();
        core::configManager.release();
    }

    void draw(void* ctx) {
        std::string items = "";
        for (std::string name : sigpath::sourceManager.sourceNames) {
            items += name;
            items += '\0';
        }
        float itemWidth = ImGui::GetContentRegionAvailWidth();

        if (gui::mainWindow.sdrIsRunning()) { style::beginDisabled(); }
        
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::Combo("##source", &sourceId, items.c_str())) {
            sigpath::sourceManager.selectSource(sigpath::sourceManager.sourceNames[sourceId]);
            core::configManager.aquire();
            core::configManager.conf["source"] = sigpath::sourceManager.sourceNames[sourceId];
            core::configManager.release(true);
        }

        if (gui::mainWindow.sdrIsRunning()) { style::endDisabled(); }

        sigpath::sourceManager.showSelectedMenu();

        ImGui::Text("Offset mode");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##_sdrpp_offset_mode", &offsetMode, offsetModesTxt)) {
            updateOffset();
            core::configManager.aquire();
            core::configManager.conf["offsetMode"] = offsetMode;
            core::configManager.release(true);
        }

        ImGui::Text("Offset");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth - ImGui::GetCursorPosX());
        if (offsetMode == OFFSET_MODE_CUSTOM) {
            if (ImGui::InputDouble("##freq_offset", &customOffset, 1.0, 100.0)) {
                updateOffset();
                core::configManager.aquire();
                core::configManager.conf["offset"] = customOffset;
                core::configManager.release(true);
            }
        }
        else {
            style::beginDisabled();
            ImGui::InputDouble("##freq_offset", &effectiveOffset, 1.0, 100.0);
            style::endDisabled();
        }
        
    }
}