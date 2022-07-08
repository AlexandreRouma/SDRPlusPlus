#pragma once
#include <math.h>
#include "tap.h"
#include "../math/constants.h"

namespace dsp::taps {
    template<class T>
    inline tap<T> rootRaisedCosine(int count, double beta, double Ts) {
        // Allocate taps
        tap<T> taps = taps::alloc<T>(count);
        
        // Generate taps
        double half = (double)count / 2.0;
        double limit = Ts / (4.0 * beta);
        for (int i = 0; i < count; i++) {
            double t = (double)i - half + 0.5;
            if (t == 0.0) {
                taps.taps[i] = (1.0 + beta*(4.0/DB_M_PI - 1.0)) / Ts;
            }
            else if (t == limit || t == -limit) {
                taps.taps[i] = ((1.0 + 2.0/DB_M_PI)*sin(DB_M_PI/(4.0*beta)) + (1.0 - 2.0/DB_M_PI)*cos(DB_M_PI/(4.0*beta))) * beta/(Ts*DB_M_SQRT2);
            }
            else {
                taps.taps[i] = ((sin((1.0 - beta)*DB_M_PI*t/Ts) + cos((1.0 + beta)*DB_M_PI*t/Ts)*4.0*beta*t/Ts) / ((1.0 - (4.0*beta*t/Ts)*(4.0*beta*t/Ts))*DB_M_PI*t/Ts)) / Ts;
            }
        }

        return taps;
    }

    template<class T>
    inline tap<T> rootRaisedCosine(int count, double beta, double symbolrate, double samplerate) {
        return rootRaisedCosine<T>(count, beta, samplerate / symbolrate);
    }
}