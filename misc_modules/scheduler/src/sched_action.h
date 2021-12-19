#pragma once
#include <json.hpp>
#include <memory>
#include <spdlog/spdlog.h>

using namespace nlohmann;

namespace sched_action {
    class ActionClass {
    public:
        virtual ~ActionClass(){};
        virtual void trigger() = 0;
        virtual void prepareEditMenu() = 0;
        virtual bool showEditMenu(bool& valid) = 0;
        virtual void loadFromConfig(json config) = 0;
        virtual json saveToConfig() = 0;
        virtual std::string getName() = 0;

        virtual bool isValid() {
            return valid;
        }

        bool selected = false;

    private:
        bool valid = false;
    };

    typedef std::shared_ptr<ActionClass> Action;
}

#include <actions/start_recorder.h>
#include <actions/tune_vfo.h>