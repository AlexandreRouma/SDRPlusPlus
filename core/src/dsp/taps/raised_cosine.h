#pragma once
#include <math.h>
#include "tap.h"
#include "../math/constants.h"
#include "../math/sinc.h"

namespace dsp::taps {
    template<class T>
    inline tap<T> raisedCosine(int count, double beta, double Ts) {
        // Allocate taps
        tap<T> taps = taps::alloc<T>(count);
        
        // Generate taps
        double half = (double)count / 2.0;
        double limit = Ts / (2.0 * beta);
        for (int i = 0; i < count; i++) {
            double t = (double)i - half + 0.5;
            if (t == limit || t == -limit) {
                taps.taps[i] = math::sinc(1.0 / (2.0*beta)) * DB_M_PI / (4.0*Ts);
            }
            else {
                taps.taps[i] = math::sinc(t / Ts) * DB_M_PI / (4.0*Ts);
            }
        }

        return taps;
    }

    template<class T>
    inline tap<T> raisedCosine(int count, double beta, double symbolrate, double samplerate) {
        return raisedCosine<T>(count, beta, samplerate / symbolrate);
    }
}