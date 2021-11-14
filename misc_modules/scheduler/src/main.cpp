#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <sched_task.h>
#include <map>

SDRPP_MOD_INFO {
    /* Name:            */ "scheduler",
    /* Description:     */ "SDR++ Scheduler",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

class DemoModule : public ModuleManager::Instance {
public:
    DemoModule(std::string name) {
        this->name = name;
        gui::menu.registerEntry(name, menuHandler, this, NULL);

        Task t;
        t.selected = false;

        json recStartConfig;
        recStartConfig["recorder"] = "Recorder";
        
        json tuneVFOConfig;
        tuneVFOConfig["vfo"] = "Radio";
        tuneVFOConfig["frequency"] = 103500000.0;

        auto recStart = sched_action::StartRecorder();
        auto tuneVFO = sched_action::TuneVFO();

        recStart->loadFromConfig(recStartConfig);
        tuneVFO->loadFromConfig(tuneVFOConfig);

        t.addAction(tuneVFO);
        t.addAction(recStart);

        tasks["Test"] = t;
        tasks["Another test"] = t;
    }

    ~DemoModule() {
        gui::menu.removeEntry(name);
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        DemoModule* _this = (DemoModule*)ctx;
        
        // If editing, show menu
        if (!_this->editedTask.empty()) {
            gui::mainWindow.lockWaterfallControls = true;
            std::string id = "Edit##scheduler_edit_task_" + _this->name;
            ImGui::OpenPopup(id.c_str());
            if (ImGui::BeginPopup(id.c_str(), ImGuiWindowFlags_NoResize)) {
                bool valid = false;
                bool open = _this->tasks[_this->editedTask].showEditMenu(_this->editedName, valid);

                // Stop editing of closed
                if (!open) {
                    // Rename if name changed and valid
                    if (valid && strcmp(_this->editedName, _this->editedTask.c_str())) {
                        Task task = _this->tasks[_this->editedTask];
                        _this->tasks.erase(_this->editedTask);
                        _this->tasks[_this->editedName] = task;
                    }

                    // Stop showing edit window
                    _this->editedTask.clear();
                }

                ImGui::EndPopup();
            }
        }

        if (ImGui::BeginTable(("freq_manager_bkm_table"+_this->name).c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Countdown");
            ImGui::TableSetupScrollFreeze(2, 1);
            ImGui::TableHeadersRow();
            for (auto& [name, bm] : _this->tasks) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImVec2 min = ImGui::GetCursorPos();

                if (ImGui::Selectable((name + "##_freq_mgr_bkm_name_" + _this->name).c_str(), &bm.selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_SelectOnClick)) {
                    // if shift or control isn't pressed, deselect all others
                    if (!ImGui::IsKeyDown(GLFW_KEY_LEFT_SHIFT) && !ImGui::IsKeyDown(GLFW_KEY_RIGHT_SHIFT) &&
                        !ImGui::IsKeyDown(GLFW_KEY_LEFT_CONTROL) && !ImGui::IsKeyDown(GLFW_KEY_RIGHT_CONTROL)) {
                        for (auto& [_name, _bm] : _this->tasks) {
                            if (name == _name) { continue; }
                            _bm.selected = false;
                        }
                    }
                }
                if (ImGui::TableGetHoveredColumn() >= 0 && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && _this->editedTask.empty()) {
                    _this->editedTask = name;
                    strcpy(_this->editedName, name.c_str());
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("todo");
            }
            ImGui::EndTable();
        }
    }

    std::string name;
    bool enabled = true;

    std::string editedTask = "";
    char editedName[1024];

    std::map<std::string, Task> tasks;

};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new DemoModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (DemoModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}