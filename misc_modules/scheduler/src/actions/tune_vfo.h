#pragma once
#include <sched_action.h>
#include <utils/freq_formatting.h>
#include <gui/tuner.h>
#include <signal_path/signal_path.h>

namespace sched_action {

    const int tuningModes[] = {
        tuner::TUNER_MODE_NORMAL,
        tuner::TUNER_MODE_CENTER
    };

    const int tuningModeCount = sizeof(tuningModes) / sizeof(int);

    const char* tuningModesStr[] = {
        "Normal",
        "Center"
    };

    class TuneVFOClass : public ActionClass {
    public:
        TuneVFOClass() {
            for (auto& mode : tuningModesStr) {
                tuningModesTxt += mode;
                tuningModesTxt += '\0';
            }
        }

        ~TuneVFOClass() {}

        void trigger() {
            if (vfoName.empty()) { return; }
            tuner::tune(tuningMode, vfoName, frequency);
        }

        void prepareEditMenu() {
            tmpFrequency = frequency;

            // TODO: Find tuning mode
            tuningModeId = 0;

            // Generate text list
            vfoNameId = -1;
            vfoNames.clear();
            vfoNamesTxt.clear();
            int id = 0;
            for (auto& [name, vfo] : gui::waterfall.vfos) {
                vfoNames.push_back(name);
                vfoNamesTxt += name;
                vfoNamesTxt += '\0';
                if (name == vfoName) {
                    vfoNameId = id;
                }
                id++;
            }

            // If VFO not found, reset the name
            if (id < 0 && !vfoNames.empty()) {
                vfoNameId = 0;
            }

            // Search ID of the tuning mode
            tuningModeId = -1;
            for (int i = 0; i < tuningModeCount; i++) {
                if (tuningModes[i] == tuningMode) {
                    tuningModeId = i;
                    break;
                }
            }

            if (tuningModeId < 0) {
                tuningModeId = 0;
            }
        }

        bool showEditMenu(bool& valid) {
            ImGui::LeftLabel("VFO");
            ImGui::SetNextItemWidth(250 - ImGui::GetCursorPosX());
            ImGui::Combo("##scheduler_action_tunevfo_edit_vfo", &vfoNameId, vfoNamesTxt.c_str());

            ImGui::LeftLabel("Frequency");
            ImGui::SetNextItemWidth(250 - ImGui::GetCursorPosX());
            ImGui::InputDouble("Hz##scheduler_action_tunevfo_edit_freq", &tmpFrequency);

            ImGui::LeftLabel("Tuning Mode");
            ImGui::SetNextItemWidth(250 - ImGui::GetCursorPosX());
            ImGui::Combo("##scheduler_action_tunevfo_edit_tmode", &tuningModeId, tuningModesTxt.c_str());

            if (ImGui::Button("Apply")) {
                vfoName = vfoNames[vfoNameId];
                frequency = tmpFrequency;
                tuningMode = tuningModes[tuningModeId];
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

        void loadFromConfig(json config) {
            if (config.contains("vfo")) { vfoName = config["vfo"]; }
            if (config.contains("frequency")) { frequency = config["frequency"]; }
            if (config.contains("tuningMode")) { tuningMode = config["tuningMode"]; }

            name = "Tune \"" + vfoName + "\" to " + utils::formatFreq(frequency);
        }

        json saveToConfig() {
            json config;
            config["vfo"] = vfoName;
            config["frequency"] = frequency;
            config["tuningMode"] = tuningMode;
            return config;
        }

        std::string getName() {
            return name;
        }

    private:
        std::string tuningModesTxt;
        std::vector<std::string> vfoNames;
        std::string vfoNamesTxt;

        std::string vfoName = "";
        double frequency = 0;
        int tuningMode = 0;

        double tmpFrequency;
        int tuningModeId;

        int vfoNameId = -1;

        std::string name;

    };

    Action TuneVFO() {
        return Action(new TuneVFOClass);
    }
}