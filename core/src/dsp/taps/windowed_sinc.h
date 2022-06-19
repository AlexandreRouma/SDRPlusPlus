#pragma once
#include "tap.h"
#include "../math/sinc.h"
#include "../math/freq_to_omega.h"
#include "../window/nuttall.h"

namespace dsp::taps {
    template<class T, typename Func>
    inline tap<T> windowedSinc(int count, double omega, Func window, double norm = 1.0) {
        // Allocate taps
        tap<T> taps = taps::alloc<T>(count);
        
        // Generate using window
        double half = (double)count / 2.0;
        double corr = norm * omega / DB_M_PI;
        
        for (int i = 0; i < count; i++) {
            double t = (double)i - half + 0.5;
            if constexpr (std::is_same_v<T, float>) {
                taps.taps[i] = math::sinc(t * omega) * window(t - half, count) * corr;
            }
            if constexpr (std::is_same_v<T, complex_t>) {
                complex_t cplx = { (float)math::sinc(t * omega), 0.0f };
                taps.taps[i] = cplx * window(t - half, count) * corr;
            }
        }

        return taps;
    }

    template<class T, typename Func>
    inline tap<T> windowedSinc(int count, double cutoff, double samplerate, Func window, double norm = 1.0) {
        return windowedSinc<T>(count, math::freqToOmega(cutoff, samplerate), window, norm);
    }
}