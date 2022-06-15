#pragma once
#include "../types.h"
#include "windowed_sinc.h"
#include "estimate_tap_count.h"
#include "../window/nuttall.h"
#include "../math/phasor.h"
#include "../math/freq_to_omega.h"

namespace dsp::taps {
    template<class T>
    inline tap<T> bandPass(double bandStart, double bandStop, double transWidth, double sampleRate) {
        assert(bandStop > bandStart);
        float offsetOmega = math::freqToOmega((bandStart + bandStop) / 2.0, sampleRate);
        return windowedSinc<T>(estimateTapCount(transWidth, sampleRate), (bandStop - bandStart) / 2.0, sampleRate, [=](double n, double N) {
            if constexpr (std::is_same_v<T, float>) {
                return cosf(offsetOmega * (float)n) * window::nuttall(n, N);
            }
            if constexpr (std::is_same_v<T, complex_t>) {
                return math::phasor(offsetOmega * (float)n) * window::nuttall(n, N);
            }
        });
    }
}