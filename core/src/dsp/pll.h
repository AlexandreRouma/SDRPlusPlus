#pragma once
#include <dsp/block.h>
#include <dsp/interpolation_taps.h>
#include <math.h>
#include <dsp/utils/macros.h>

namespace dsp {
    template <int ORDER>
    class CostasLoop: public generic_block<CostasLoop<ORDER>> {
    public:
        CostasLoop() {}
        CostasLoop(stream<complex_t>* in, float loopBandwidth) { init(in, loopBandwidth); }

        void init(stream<complex_t>* in, float loopBandwidth) {
            _in = in;
            lastVCO.re = 1.0f;
            lastVCO.im = 0.0f;
            _loopBandwidth = loopBandwidth;

            float dampningFactor = sqrtf(2.0f) / 2.0f;
            float denominator = (1.0 + 2.0 * dampningFactor * _loopBandwidth + _loopBandwidth * _loopBandwidth);
            _alpha = (4 * dampningFactor * _loopBandwidth) / denominator;
            _beta = (4 * _loopBandwidth * _loopBandwidth) / denominator;

            generic_block<CostasLoop<ORDER>>::registerInput(_in);
            generic_block<CostasLoop<ORDER>>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            generic_block<CostasLoop<ORDER>>::tempStop();
            generic_block<CostasLoop<ORDER>>::unregisterInput(_in);
            _in = in;
            generic_block<CostasLoop<ORDER>>::registerInput(_in);
            generic_block<CostasLoop<ORDER>>::tempStart();
        }

        void setLoopBandwidth(float loopBandwidth) {
            generic_block<CostasLoop<ORDER>>::tempStop();
            _loopBandwidth = loopBandwidth;
            float dampningFactor = sqrtf(2.0f) / 2.0f;
            float denominator = (1.0 + 2.0 * dampningFactor * _loopBandwidth + _loopBandwidth * _loopBandwidth);
            _alpha = (4 * dampningFactor * _loopBandwidth) / denominator;
            _beta = (4 * _loopBandwidth * _loopBandwidth) / denominator;
            generic_block<CostasLoop<ORDER>>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            complex_t outVal;
            float error;

            for (int i = 0; i < count; i++) {

                // Mix the VFO with the input to create the output value
                outVal = lastVCO * _in->readBuf[i];
                out.writeBuf[i] = outVal;

                // Calculate the phase error estimation
                if constexpr (ORDER == 2) {
                    error = outVal.re * outVal.im;
                }
                if constexpr (ORDER == 4) {
                    error = (DSP_STEP(outVal.re) * outVal.im) - (DSP_STEP(outVal.im) * outVal.re);
                }
                if constexpr (ORDER == 8) {
                    // This is taken from GR, I have no idea how it works but it does...
                    const float K = (sqrtf(2.0) - 1);
                    if (fabsf(outVal.re) >= fabsf(outVal.im)) {
                        error = ((outVal.re > 0.0f ? 1.0f : -1.0f) * outVal.im -
                                (outVal.im > 0.0f ? 1.0f : -1.0f) * outVal.re * K);
                    } else {
                        error = ((outVal.re > 0.0f ? 1.0f : -1.0f) * outVal.im * K -
                                (outVal.im > 0.0f ? 1.0f : -1.0f) * outVal.re);
                    }
                }
                
                if (error > 1.0f) { error = 1.0f; }
                if (error < -1.0f) { error = -1.0f; }
                
                // Integrate frequency and clamp it
                vcoFrequency += _beta * error;
                if (vcoFrequency > 1.0f) { vcoFrequency = 1.0f; }
                if (vcoFrequency < -1.0f) { vcoFrequency = -1.0f; }

                // Calculate new phase and wrap it
                vcoPhase += vcoFrequency + (_alpha * error);
                while (vcoPhase > (2.0f * FL_M_PI)) { vcoPhase -= (2.0f * FL_M_PI); }
                while (vcoPhase < (-2.0f * FL_M_PI)) { vcoPhase += (2.0f * FL_M_PI); }

                // Calculate output
                lastVCO.re = cosf(vcoPhase);
                lastVCO.im = sinf(vcoPhase);

            }
            
            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        float _loopBandwidth = 1.0f;

        float _alpha; // Integral coefficient
        float _beta; // Proportional coefficient
        float vcoFrequency = 0.0f;
        float vcoPhase = 0.0f;
        complex_t lastVCO;

        stream<complex_t>* _in;

    };
}