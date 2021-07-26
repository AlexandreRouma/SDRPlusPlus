#include <gui/menus/module_manager.h>
#include <imgui.h>
#include <core.h>
#include <string.h>
#include <gui/style.h>

namespace module_manager_menu {
    char modName[1024];
    std::vector<std::string> modTypes;
    std::vector<std::string> toBeRemoved;
    std::string modTypesTxt;
    int modTypeId;

    void init() {
        modName[0] = 0;

        modTypes.clear();
        modTypesTxt = "";
        for (auto& [name, mod] : core::moduleManager.modules) {
            modTypes.push_back(name);
            modTypesTxt += name;
            modTypesTxt += '\0';
        }
        modTypeId = 0;
    }

    void draw(void* ctx) {
        bool modified = false;
        if (ImGui::BeginTable("Module Manager Table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 10);
            ImGui::TableHeadersRow();

            float height = ImGui::CalcTextSize("-").y;

            toBeRemoved.clear();

            for (auto& [name, inst] : core::moduleManager.instances) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text(name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text(inst.module.info->name);

                ImGui::TableSetColumnIndex(2);
                ImVec2 origPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(origPos.x - 3, origPos.y));
                if (ImGui::Button(("##module_mgr_"+name).c_str(), ImVec2(height,height))) {
                    toBeRemoved.push_back(name);
                    modified = true;
                }
                ImGui::SetCursorPos(ImVec2(origPos.x + 2, origPos.y - 5));
                ImGui::Text("_");
            }
            ImGui::EndTable();

            for (auto& rem : toBeRemoved) {
                core::moduleManager.deleteInstance(rem);
            }
        }
       

        // Add module row with slightly different settings
        ImGui::BeginTable("Module Manager Add Table", 3);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 16);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        ImGui::InputText("##module_mod_name", modName, 1000);

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        ImGui::Combo("##module_mgr_type", &modTypeId, modTypesTxt.c_str());

        ImGui::TableSetColumnIndex(2);
        if (strlen(modName) == 0) { style::beginDisabled(); }
        if (ImGui::Button("+##module_mgr_add_btn", ImVec2(16,0))) {
            core::moduleManager.createInstance(modName, modTypes[modTypeId]);
            core::moduleManager.postInit(modName);
            modified = true;
        }
        if (strlen(modName) == 0) { style::endDisabled(); }
        ImGui::EndTable();

        if (modified) {
            // Update enabled and disabled modules
            core::configManager.acquire();
            json instances;
            for (auto [_name, inst] : core::moduleManager.instances) {
                instances[_name]["module"] = inst.module.info->name;
                instances[_name]["enabled"] = inst.instance->isEnabled();
            }
            core::configManager.conf["moduleInstances"] = instances;
            core::configManager.release(true);
        }
    }
}