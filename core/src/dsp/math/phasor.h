#pragma once
#include <math.h>
#include "../types.h"

namespace dsp::math {
    inline complex_t phasor(float x) {
        complex_t cplx = { cosf(x), sinf(x) };
        return cplx;
    }
}