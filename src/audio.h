#pragma once
#include <dsp/stream.h>

namespace audio {
    void registerStream(dsp::stream<float>* stream, std::string name, std::string vfoName);
};