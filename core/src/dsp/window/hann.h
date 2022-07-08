#pragma once
#include "cosine.h"

namespace dsp::window {
    inline double hann(double n, double N) {
        const double coefs[] = { 0.5, 0.5 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}