#pragma once
#include "../processor.h"

namespace dsp::noise_reduction {
    class NoiseBlanker : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        NoiseBlanker() {}

        NoiseBlanker(stream<complex_t>* in, double rate, double level) { init(in, rate, level); }

        void init(stream<complex_t>* in, double rate, double level) {
            _rate = rate;
            _invRate = 1.0f - _rate;
            _level = level;
            base_type::init(in);
        }

        void setRate(double rate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _rate = rate;
            _invRate = 1.0f - _rate;
        }

        void setLevel(double level) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _level = level;
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            amp = 1.0f;
        }

        inline int process(int count, complex_t* in, complex_t* out) {
            for (int i = 0; i < count; i++) {
                // Get signal amplitude
                float inAmp = in[i].amplitude();

                // Update average amplitude
                float gain = 1.0f;
                if (inAmp != 0.0f) {
                    amp = (amp * _invRate) + (inAmp * _rate);
                    float excess = inAmp / amp;
                    if (excess > _level) {
                        gain = 1.0f / excess;
                    }
                }
                
                // Scale output by gain
                out[i] = in[i] * gain;
            }
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        float _rate;
        float _invRate;
        float _level;

        float amp = 1.0;

    };
}