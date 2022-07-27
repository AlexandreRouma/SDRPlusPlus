#pragma once
#include "../types.h"
#include "windowed_sinc.h"
#include "estimate_tap_count.h"
#include "../window/nuttall.h"
#include "../math/phasor.h"
#include "../math/hz_to_rads.h"

namespace dsp::taps {
    template<class T>
    inline tap<T> bandPass(double bandStart, double bandStop, double transWidth, double sampleRate, bool oddTapCount = false) {
        assert(bandStop > bandStart);
        float offsetOmega = math::hzToRads((bandStart + bandStop) / 2.0, sampleRate);
        int count = estimateTapCount(transWidth, sampleRate);
        if (oddTapCount && !(count % 2)) { count++; }
        return windowedSinc<T>(count, (bandStop - bandStart) / 2.0, sampleRate, [=](double n, double N) {
            if constexpr (std::is_same_v<T, float>) {
                return cosf(offsetOmega * (float)n) * window::nuttall(n, N);
            }
            if constexpr (std::is_same_v<T, complex_t>) {
                // The offset is negative to flip the taps. Complex bandpass are asymetric
                return math::phasor(-offsetOmega * (float)n) * window::nuttall(n, N);
            }
        });
    }
}