#pragma once
#include <signal_path/dsp.h>
#include <signal_path/vfo_manager.h>
#include <signal_path/source.h>
#include <module.h>

namespace sigpath {
    SDRPP_EXPORT SignalPath signalPath;
    SDRPP_EXPORT VFOManager vfoManager;
    SDRPP_EXPORT SourceManager sourceManager;
};