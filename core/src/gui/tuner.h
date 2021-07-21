#pragma once
#include <string>
#include <module.h>

namespace tuner {
    void centerTuning(std::string vfoName, double freq);
    void normalTuning(std::string vfoName, double freq);
    void iqTuning(double freq);

    enum {
        TUNER_MODE_CENTER,
        TUNER_MODE_NORMAL,
        TUNER_MODE_LOWER_HALF,
        TUNER_MODE_UPPER_HALF,
        TUNER_MODE_IQ_ONLY,
        _TUNER_MODE_COUNT
    };

    void tune(int mode, std::string vfoName, double freq);
}