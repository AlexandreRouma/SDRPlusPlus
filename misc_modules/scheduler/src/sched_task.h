#pragma once
#include <vector>
#include <imgui.h>
#include <gui/style.h>
#include <sched_action.h>

class Task {
public:
    void trigger() {
        for (auto& act : actions) {
            act->trigger();
        }
    }

    void addAction(sched_action::Action act) {
        actions.push_back(act);
    }

    bool removeAction(sched_action::Action act) {
        auto it = std::find(actions.begin(), actions.end(), act);
        if (it == actions.end()) { return false; }
        actions.erase(it);
        return true;
    }

    bool showEditMenu(char* name, bool& valid) {
        ImGui::LeftLabel("Name");
        ImGui::InputText("##scheduler_task_edit_name", name, 1023);

        if (ImGui::BeginTable("scheduler_task_triggers", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 100))) {
            ImGui::TableSetupColumn("Triggers");
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();
            
            // Fill rows here
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Every day at 00:00:00");

            ImGui::EndTable();
        }

        if (ImGui::BeginTable("scheduler_task_actions", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 100))) {
            ImGui::TableSetupColumn("Actions");
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();

            for (auto& act : actions) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(act->getName().c_str());
            }

            ImGui::EndTable();
        }

        if (ImGui::Button("Apply")) {
            valid = true;
            return false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            valid = false;
            return false;
        }

        return true;
    }

    bool selected;

private:
    std::vector<sched_action::Action> actions;

};