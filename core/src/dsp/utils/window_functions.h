#pragma once
#include <dsp/types.h>

namespace dsp {
    namespace window_function {
        inline float blackman(float n, float N, float alpha = 0.16f) {
            float a0 = (1.0f-alpha) / 2.0f;
            float a2 = alpha / 2.0f;
            return a0 - (0.5f*cosf(2.0f*FL_M_PI*(n/N))) + (a2*cosf(4.0f*FL_M_PI*(n/N)));
        }

        inline float blackmanThirdOrder(float n, float N, float a0, float a1, float a2, float a3) {
            return a0 - (a1*cosf(2.0f*FL_M_PI*(n/N))) + (a2*cosf(4.0f*FL_M_PI*(n/N))) - (a3*cosf(6.0f*FL_M_PI*(n/N)));
        }

        inline float nuttall(float n, float N) {
            return blackmanThirdOrder(n, N, 0.3635819f, 0.4891775f, 0.1365995f, 0.0106411f);
        }

        inline float blackmanNuttall(float n, float N) {
            return blackmanThirdOrder(n, N, 0.3635819f, 0.4891775f, 0.1365995f, 0.0106411f);
        }

        inline float blackmanHarris(float n, float N) {
            return blackmanThirdOrder(n, N, 0.35875f, 0.48829f, 0.14128f, 0.01168f);
        }
    }
}