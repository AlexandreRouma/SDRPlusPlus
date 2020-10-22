#pragma once
#include <module.h>
#include <config.h>
#include <scripting.h>

namespace core {
    SDRPP_EXPORT ConfigManager configManager;
    SDRPP_EXPORT ScriptManager scriptManager;

    void setInputSampleRate(double samplerate);
};

int sdrpp_main();