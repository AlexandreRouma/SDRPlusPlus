#pragma once
#include <math.h>
#include "constants.h"

#define FAST_ATAN2_COEF1 FL_M_PI / 4.0f
#define FAST_ATAN2_COEF2 3.0f * FAST_ATAN2_COEF1

namespace dsp::math {
    inline float fastAtan2(float x, float y) {
        float abs_y = fabsf(y);
        float r, angle;
        if (x == 0.0f && y == 0.0f) { return 0.0f; }
        if (x >= 0.0f) {
            r = (x - abs_y) / (x + abs_y);
            angle = FAST_ATAN2_COEF1 - FAST_ATAN2_COEF1 * r;
        }
        else {
            r = (x + abs_y) / (abs_y - x);
            angle = FAST_ATAN2_COEF2 - FAST_ATAN2_COEF1 * r;
        }
        if (y < 0.0f) {
            return -angle;
        }
        return angle;
    }
}