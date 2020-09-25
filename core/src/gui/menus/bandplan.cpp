#include <gui/menus/bandplan.h>
#include <gui/bandplan.h>
#include <gui/gui.h>
#include <core.h>

namespace bandplanmenu {
    int bandplanId;
    bool bandPlanEnabled;

    void init() {
        // todo: check if the bandplan wasn't removed
        if (bandplan::bandplanNames.size() == 0) {
            gui::waterfall.hideBandplan();
            return;
        }

        if (bandplan::bandplans.find(core::configManager.conf["bandPlan"]) != bandplan::bandplans.end()) {
            std::string name = core::configManager.conf["bandPlan"];
            bandplanId = std::distance(bandplan::bandplanNames.begin(), std::find(bandplan::bandplanNames.begin(),
                                    bandplan::bandplanNames.end(), name));
            gui::waterfall.bandplan = &bandplan::bandplans[name];
        }
        else {
            gui::waterfall.bandplan = &bandplan::bandplans[bandplan::bandplanNames[0]];
        }

        bandPlanEnabled = core::configManager.conf["bandPlanEnabled"];
        bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
    }

    void draw(void* ctx) {
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();
        ImGui::PushItemWidth(menuColumnWidth);
        if (ImGui::Combo("##_4_", &bandplanId, bandplan::bandplanNameTxt.c_str())) {
            gui::waterfall.bandplan = &bandplan::bandplans[bandplan::bandplanNames[bandplanId]];
            core::configManager.aquire();
            core::configManager.conf["bandPlan"] = bandplan::bandplanNames[bandplanId];
            core::configManager.release(true);
        }
        ImGui::PopItemWidth();
        if (ImGui::Checkbox("Enabled", &bandPlanEnabled)) {
            bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
            core::configManager.aquire();
            core::configManager.conf["bandPlanEnabled"] = bandPlanEnabled;
            core::configManager.release(true);
        }
        bandplan::BandPlan_t plan = bandplan::bandplans[bandplan::bandplanNames[bandplanId]];
        ImGui::Text("Country: %s (%s)", plan.countryName.c_str(), plan.countryCode.c_str());
        ImGui::Text("Author: %s", plan.authorName.c_str());
        ImGui::Spacing();
    }
};