#pragma once
#include <math.h>
#include <assert.h>
#include "../math/constants.h"

namespace dsp::window {
    inline double cosine(double n, double N, const double* coefs, int coefCount) {
        assert(coefCount > 0);
        double win = 0.0;
        double sign = 1.0;
        for (int i = 0; i < coefCount; i++) {
            win += sign * coefs[i] * cos((double)i * 2.0 * DB_M_PI * n / N);
            sign = -sign;
        }
        return win;
    } 
}