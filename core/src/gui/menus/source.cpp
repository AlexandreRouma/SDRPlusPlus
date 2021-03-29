#include <gui/menus/source.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>

namespace sourecmenu {
    int sourceId = 0;
    double freqOffset = 0.0;

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
        freqOffset = core::configManager.conf["offset"];
        sigpath::sourceManager.setTuningOffset(freqOffset);
        core::configManager.release();
    }

    void draw(void* ctx) {
        std::string items = "";
        for (std::string name : sigpath::sourceManager.sourceNames) {
            items += name;
            items += '\0';
        }
        float itemWidth = ImGui::GetContentRegionAvailWidth();

        if (sdrIsRunning()) { style::beginDisabled(); }
        
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::Combo("##source", &sourceId, items.c_str())) {
            sigpath::sourceManager.selectSource(sigpath::sourceManager.sourceNames[sourceId]);
            core::configManager.aquire();
            core::configManager.conf["source"] = sigpath::sourceManager.sourceNames[sourceId];
            core::configManager.release(true);
        }

        if (sdrIsRunning()) { style::endDisabled(); }

        sigpath::sourceManager.showSelectedMenu();

        ImGui::SetNextItemWidth(itemWidth - ImGui::CalcTextSize("Offset (Hz)").x - 10);
        if (ImGui::InputDouble("Offset (Hz)##freq_offset", &freqOffset, 1.0, 100.0)) {
            sigpath::sourceManager.setTuningOffset(freqOffset);
            core::configManager.aquire();
            core::configManager.conf["offset"] = freqOffset;
            core::configManager.release(true);
        }
    }
}