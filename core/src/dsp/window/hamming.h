#pragma once
#include "cosine.h"

namespace dsp::window {
    inline double hamming(double n, double N) {
        const double coefs[] = { 0.54, 0.46 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}