#include <gui/menus/bandplan.h>
#include <gui/widgets/bandplan.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/style.h>

namespace bandplanmenu {
    int bandplanId;
    bool bandPlanEnabled;
    int bandPlanPos = 0;

    const char* bandPlanPosTxt = "Bottom\0Top\0";

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
        bandPlanPos = core::configManager.conf["bandPlanPos"];
        gui::waterfall.setBandPlanPos(bandPlanPos);
    }

    void draw(void* ctx) {
        float menuColumnWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushItemWidth(menuColumnWidth);
        if (ImGui::Combo("##_bandplan_name_", &bandplanId, bandplan::bandplanNameTxt.c_str())) {
            gui::waterfall.bandplan = &bandplan::bandplans[bandplan::bandplanNames[bandplanId]];
            core::configManager.acquire();
            core::configManager.conf["bandPlan"] = bandplan::bandplanNames[bandplanId];
            core::configManager.release(true);
        }
        ImGui::PopItemWidth();

        ImGui::LeftLabel("Position");
        ImGui::SetNextItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##_bandplan_pos_", &bandPlanPos, bandPlanPosTxt)) {
            gui::waterfall.setBandPlanPos(bandPlanPos);
            core::configManager.acquire();
            core::configManager.conf["bandPlanPos"] = bandPlanPos;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("Enabled", &bandPlanEnabled)) {
            bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
            core::configManager.acquire();
            core::configManager.conf["bandPlanEnabled"] = bandPlanEnabled;
            core::configManager.release(true);
        }
        bandplan::BandPlan_t plan = bandplan::bandplans[bandplan::bandplanNames[bandplanId]];
        ImGui::Text("Country: %s (%s)", plan.countryName.c_str(), plan.countryCode.c_str());
        ImGui::Text("Author: %s", plan.authorName.c_str());
    }
};