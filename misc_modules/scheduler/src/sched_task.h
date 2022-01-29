#pragma once
#include <vector>
#include <imgui.h>
#include <gui/style.h>
#include <sched_action.h>
#include <keybinds.h>

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

    void prepareEditMenu() {
        for (auto& act : actions) {
            act->selected = false;
        }
    }

    bool showEditMenu(char* name, bool& valid) {
        ImGui::LeftLabel("Name");
        ImGui::InputText("##scheduler_task_edit_name", name, 1023);

        if (editedAction >= 0) {
            bool valid = false;

            std::string id = "Edit Action##scheduler_edit_action";
            ImGui::OpenPopup(id.c_str());
            if (ImGui::BeginPopup(id.c_str(), ImGuiWindowFlags_NoResize)) {
                bool valid = false;
                bool open = actions[editedAction]->showEditMenu(valid);

                // Stop editing of closed
                if (!open) {
                    // TODO: Do something if valid I think
                    editedAction = -1;
                }

                ImGui::EndPopup();
            }
        }

        if (ImGui::BeginTable("scheduler_task_triggers", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 100))) {
            ImGui::TableSetupColumn("Triggers");
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();

            // Fill rows here
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Every day at 00:00:00");

            ImGui::EndTable();
        }

        if (ImGui::BeginTable("scheduler_task_actions", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 100))) {
            ImGui::TableSetupColumn("Actions");
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();

            int id = 0;
            for (auto& act : actions) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (ImGui::Selectable((act->getName() + "##scheduler_task_actions_entry").c_str(), &act->selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_SelectOnClick)) {
                    // if shift or control isn't pressed, deselect all others
                    if (!ImGui::IsKeyDown(KB_KEY_LSHIFT) && !ImGui::IsKeyDown(KB_KEY_RSHIFT) &&
                        !ImGui::IsKeyDown(KB_KEY_LCTRL) && !ImGui::IsKeyDown(KB_KEY_RCTRL)) {
                        int _id = 0;
                        for (auto& _act : actions) {
                            if (_id == id) { continue; }
                            _act->selected = false;
                            _id++;
                        }
                    }
                }

                // Open edit menu on double click
                if (ImGui::TableGetHoveredColumn() >= 0 && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && editedAction < 0) {
                    editedAction = id;
                    act->prepareEditMenu();
                }

                id++;
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

    int editedAction = -1;
};