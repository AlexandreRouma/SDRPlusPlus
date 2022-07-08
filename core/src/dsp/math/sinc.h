#pragma once
#include <math.h>

namespace dsp::math {
    inline double sinc(double x) {
        return (x == 0.0) ? 1.0 : (sin(x) / x);
    }
}