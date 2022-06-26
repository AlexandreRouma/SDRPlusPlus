#pragma once
#include "pll.h"

namespace dsp::loop {
    class CarrierTrackingPLL : public PLL {
        using base_type = PLL;
    public:
        inline int process(int count, complex_t* in, complex_t* out) {
            for (int i = 0; i < count; i++) {
                out[i] = in[i] * math::phasor(-pcl.phase);
                pcl.advance(math::normPhaseDiff(in[i].phase() - pcl.phase));
            }
            return count;
        }
    };
}