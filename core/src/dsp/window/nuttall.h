#pragma once
#include "cosine.h"

namespace dsp::window {
    inline double nuttall(double n, double N) {
        const double coefs[] = { 0.355768, 0.487396, 0.144232, 0.012604 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}