#pragma once
#include <dsp/types.h>

namespace dsp {
    namespace window_function {
        inline double blackman(double n, double N, double alpha = 0.16f) {
            double a0 = (1.0f-alpha) / 2.0f;
            double a2 = alpha / 2.0f;
            return a0 - (0.5f*cos(2.0f*FL_M_PI*(n/N))) + (a2*cos(4.0f*FL_M_PI*(n/N)));
        }

        inline double blackmanThirdOrder(double n, double N, double a0, double a1, double a2, double a3) {
            return a0 - (a1*cos(2.0f*FL_M_PI*(n/N))) + (a2*cos(4.0f*FL_M_PI*(n/N))) - (a3*cos(6.0f*FL_M_PI*(n/N)));
        }

        inline double nuttall(double n, double N) {
            return blackmanThirdOrder(n, N, 0.3635819f, 0.4891775f, 0.1365995f, 0.0106411f);
        }

        inline double blackmanNuttall(double n, double N) {
            return blackmanThirdOrder(n, N, 0.3635819f, 0.4891775f, 0.1365995f, 0.0106411f);
        }

        inline double blackmanHarris(double n, double N) {
            return blackmanThirdOrder(n, N, 0.35875f, 0.48829f, 0.14128f, 0.01168f);
        }
    }
}