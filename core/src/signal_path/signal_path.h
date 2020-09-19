#pragma once
#include <signal_path/dsp.h>
#include <signal_path/vfo_manager.h>
#include <module.h>

namespace sigpath {
    SDRPP_EXPORT SignalPath signalPath;
    SDRPP_EXPORT VFOManager vfoManager;
};