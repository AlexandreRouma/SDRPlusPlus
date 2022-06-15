#pragma once
#include "cosine.h"

namespace dsp::window {
    inline double blackman(double n, double N) {
        const double coefs[] = { 0.42, 0.5, 0.08 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}