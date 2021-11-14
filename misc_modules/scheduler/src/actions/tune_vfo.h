#pragma once
#include <sched_action.h>
#include <utils/freq_formatting.h>

namespace sched_action {
    class TuneVFOClass : public ActionClass {
    public:
        TuneVFOClass() {}
        ~TuneVFOClass() {}

        void trigger() {

        }

        void prepareEditMenu() {}

        void validateEditMenu() {}

        void showEditMenu() {
            
        }

        void loadFromConfig(json config) {
            if (config.contains("vfo")) { vfoName = config["vfo"]; }
            if (config.contains("frequency")) { frequency = config["frequency"]; }
        }

        json saveToConfig() {
            json config;
            config["vfo"] = vfoName;
            config["frequency"] = frequency;
            return config;
        }

        std::string getName() {
            return "Tune \"" + vfoName + "\" to " + utils::formatFreq(frequency);
        }

        bool isValid() {
            return valid;
        }

    private:
        std::string vfoName;
        double frequency;
        bool valid = false;

    };

    Action TuneVFO() {
        return Action(new TuneVFOClass);
    }
}