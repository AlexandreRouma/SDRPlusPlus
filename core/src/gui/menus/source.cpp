#include <gui/menus/source.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>
#include <signal_path/signal_path.h>

namespace sourecmenu {
    int sourceId = 0;
    double freqOffset = 0.0;

    void init() {
        // Select default
        // TODO:  Replace by setting
        if (sigpath::sourceManager.sourceNames.size() > 0) {
            sigpath::sourceManager.selectSource(sigpath::sourceManager.sourceNames[0]);
        }
        sigpath::sourceManager.setTuningOffset(0);
    }

    void draw(void* ctx) {
        std::string items = "";
        for (std::string name : sigpath::sourceManager.sourceNames) {
            items += name;
            items += '\0';
        }
        float itemWidth = ImGui::GetContentRegionAvailWidth();
        
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::Combo("##source", &sourceId, items.c_str())) {
            sigpath::sourceManager.selectSource(sigpath::sourceManager.sourceNames[sourceId]);
        }

        sigpath::sourceManager.showSelectedMenu();
        ImGui::SetNextItemWidth(itemWidth - ImGui::CalcTextSize("Offset (Hz)").x - 10);
        if (ImGui::InputDouble("Offset (Hz)##freq_offset", &freqOffset, 1.0, 100.0)) {
            sigpath::sourceManager.setTuningOffset(freqOffset);
        }
    }
}