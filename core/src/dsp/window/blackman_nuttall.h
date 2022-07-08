#pragma once
#include <math.h>
#include "cosine.h"

namespace dsp::window {
    inline double blackmanNuttall(double n, double N) {
        const double coefs[] = { 0.3635819, 0.4891775, 0.1365995, 0.0106411 };
        return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
    }
}