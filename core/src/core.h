#pragma once
#include <module.h>
#include <config.h>

namespace core {
    SDRPP_EXPORT ConfigManager configManager;

    void setInputSampleRate(double samplerate);
};

int sdrpp_main();