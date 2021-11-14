#pragma once
#include <sched_action.h>

namespace sched_action {
    class StartRecorderClass : public ActionClass {
    public:
        StartRecorderClass() {}
        ~StartRecorderClass() {}

        void trigger() {

        }

        void showEditMenu() {
            
        }

        void loadFromConfig(json config) {
            if (config.contains("recorder")) { recorderName = config["recorder"]; }
        }

        json saveToConfig() {
            json config;
            config["recorder"] = recorderName;
            return config;
        }


        std::string getName() {
            return "Start \"" + recorderName + "\"";
        }

        bool isValid() {
            return valid;
        }

    private:
        std::string recorderName;
        bool valid = false;

    };

    Action StartRecorder() {
        return Action(new StartRecorderClass);
    }
}