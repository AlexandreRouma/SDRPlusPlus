#include <gui/menus/module_manager.h>
#include <imgui.h>
#include <core.h>
#include <string.h>
#include <gui/style.h>

namespace module_manager_menu {
    char modName[1024];
    std::vector<std::string> modTypes;
    std::string modTypesTxt;
    int modTypeId;

    void init() {
        modName[0] = 0;

        modTypes.clear();
        modTypesTxt = "";
        for (auto& [name, mod] : core::moduleManager.modules) {
            // TEMPORARY EXCLUSION FOR SOURCES AND SINKS
            if (name.find("source") != std::string::npos) { continue; }
            if (name.find("sink") != std::string::npos) { continue; }
            if (name.find("recorder") != std::string::npos) { continue; }
            if (name.find("discord") != std::string::npos) { continue; }
            modTypes.push_back(name);
            modTypesTxt += name;
            modTypesTxt += '\0';
        }
        modTypeId = 0;
    }

    void draw(void* ctx) {
        ImGui::BeginTable("Module Manager Table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 16);
        ImGui::TableHeadersRow();
        for (auto& [name, inst] : core::moduleManager.instances) {
            // TEMPORARY EXCLUSION FOR SOURCES AND SINKS
            std::string type = inst.module.info->name;
            if (type.find("source") != std::string::npos) { continue; }
            if (type.find("sink") != std::string::npos) { continue; }
            if (type.find("recorder") != std::string::npos) { continue; }
            if (type.find("discord") != std::string::npos) { continue; }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text(name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text(inst.module.info->name);

            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button(("-##module_mgr_"+name).c_str(), ImVec2(16,0))) {
                core::moduleManager.deleteInstance(name);
            }
        }

        // Add module row
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
        }
        if (strlen(modName) == 0) { style::endDisabled(); }

        ImGui::EndTable();
    }
}