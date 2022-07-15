#pragma once
#include <dsp/loop/pll.h>
#include <dsp/math/step.h>

namespace dsp::loop {
    class MeteorCostas : public PLL {
        using base_type = PLL;
    public:
        MeteorCostas() {}

        MeteorCostas(stream<complex_t>* in, double bandwidth, bool brokenModulation, double initPhase = 0.0, double initFreq = 0.0, double minFreq = -FL_M_PI, double maxFreq = FL_M_PI) { init(in, bandwidth, brokenModulation, initFreq, initPhase, minFreq, maxFreq); }

        void init(stream<complex_t>* in, double bandwidth, bool brokenModulation, double initPhase = 0.0, double initFreq = 0.0, double minFreq = -FL_M_PI, double maxFreq = FL_M_PI) {
            _broken = brokenModulation;
            base_type::init(in, bandwidth, initPhase, initFreq, minFreq, maxFreq);
        }

        void setBrokenModulation(bool enabled) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _broken = enabled;
        }

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
            if (_broken) {
                const float PHASE1 = 0.47439988279190737;
                const float PHASE2 = 2.1777839908413044;
                const float PHASE3 = 3.8682349942715186;
                const float PHASE4 = -0.29067248091319986;

                float phase = val.phase();
                float dp1 = math::normPhaseDiff(phase - PHASE1);
                float dp2 = math::normPhaseDiff(phase - PHASE2);
                float dp3 = math::normPhaseDiff(phase - PHASE3);
                float dp4 = math::normPhaseDiff(phase - PHASE4);
                float lowest = dp1;
                if (fabsf(dp2) < fabsf(lowest)) { lowest = dp2; }
                if (fabsf(dp3) < fabsf(lowest)) { lowest = dp3; }
                if (fabsf(dp4) < fabsf(lowest)) { lowest = dp4; }
                err = lowest * val.amplitude();
            }
            else {
                err = (math::step(val.re) * val.im) - (math::step(val.im) * val.re);
            }
            return std::clamp<float>(err, -1.0f, 1.0f);
        }

        bool _broken;
    };
}