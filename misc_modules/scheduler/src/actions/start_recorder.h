#pragma once
#include <sched_action.h>

namespace sched_action {
    class StartRecorderClass : public ActionClass {
    public:
        StartRecorderClass() {}
        ~StartRecorderClass() {}

        void trigger() {

        }

        void prepareEditMenu() {

        }

        bool showEditMenu(bool& valid) {
            valid = false;
            return false;
        }

        void loadFromConfig(json config) {
            if (config.contains("recorder")) { recorderName = config["recorder"]; }
            name = "Start \"" + recorderName + "\"";
        }

        json saveToConfig() {
            json config;
            config["recorder"] = recorderName;
            return config;
        }


        std::string getName() {
            return name;
        }

    private:
        std::string recorderName;

        std::string name = "Start \"\"";

    };

    Action StartRecorder() {
        return Action(new StartRecorderClass);
    }
}