#pragma once
#include "pll.h"
#include "../math/step.h"

namespace dsp::loop {
    template<int ORDER>
    class Costas : public PLL {
        static_assert(ORDER == 2 || ORDER == 4 || ORDER == 8, "Invalid costas order");
        using base_type = PLL;
    public:
        inline int process(int count, complex_t* in, complex_t* out) {
            for (int i = 0; i < count; i++) {
                out[i] = in[i] * math::phasor(-pcl.phase);
                pcl.advance(errorFunction(out[i]));
            }
            return count;
        }

    protected:
        inline float errorFunction(complex_t val) {
            float err;
            if constexpr (ORDER == 2) {
                err = val.re * val.im;
            }
            if constexpr (ORDER == 4) {
                err = (math::step(val.re) * val.im) - (math::step(val.im) * val.re);
            }
            if constexpr (ORDER == 8) {
                // The way this works is it compresses order 4 constellations into the quadrants
                const float K = sqrtf(2.0) - 1.0;
                if (fabsf(val.re) >= fabsf(val.im)) {
                    err = (math::step(val.re) * val.im) - (math::step(val.im) * val.re * K);
                }
                else {
                    err = (math::step(val.re) * val.im * K) - (math::step(val.im) * val.re);
                }
            }
            return std::clamp<float>(err, -1.0f, 1.0f);
        }
    };
}