#pragma once
#include <config.h>
#include <module.h>
#include <scripting.h>
#include <module.h>

namespace core {
    SDRPP_EXPORT ConfigManager configManager;
    SDRPP_EXPORT ScriptManager scriptManager;
    SDRPP_EXPORT ModuleManager moduleManager;

    void setInputSampleRate(double samplerate);
};

int sdrpp_main(int argc, char *argv[]);