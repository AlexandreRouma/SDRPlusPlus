#pragma once
#include "windowed_sinc.h"
#include "estimate_tap_count.h"
#include "../window/nuttall.h"

namespace dsp::taps {
    inline tap<float> lowPass(double cutoff, double transWidth, double sampleRate) {
        return windowedSinc<float>(estimateTapCount(transWidth, sampleRate), cutoff, sampleRate, window::nuttall);
    }
}