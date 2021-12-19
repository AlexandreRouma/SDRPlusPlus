#pragma once
#include <math.h>

#define FL_M_PI 3.1415926535f

namespace dsp {
    namespace math {
        inline double sinc(double omega, double x, double norm) {
            return (x == 0.0f) ? 1.0f : (sin(omega * x) / (norm * x));
        }
    }
}