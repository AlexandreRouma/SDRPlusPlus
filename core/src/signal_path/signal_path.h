#pragma once
#include "iq_frontend.h"
#include "vfo_manager.h"
#include "source.h"
#include "sink.h"
#include <module.h>

namespace sigpath {
    SDRPP_EXPORT IQFrontEnd iqFrontEnd;
    SDRPP_EXPORT VFOManager vfoManager;
    SDRPP_EXPORT SourceManager sourceManager;
    SDRPP_EXPORT SinkManager sinkManager;
};