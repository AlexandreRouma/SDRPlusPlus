#pragma once
#include <math.h>

#define FL_M_PI     3.1415926535f

namespace dsp {
    namespace math {
        inline float sinc(float omega, float x, float norm) {
            return (x == 0.0f) ? 1.0f : (sinf(omega*x)/(norm*x));
        }
    }
}