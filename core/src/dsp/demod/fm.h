#pragma once
#include "../processor.h"
#include "../math/fast_atan2.h"
#include "../math/freq_to_omega.h"
#include "../math/norm_phase_diff.h"

namespace dsp::demod {
    class FM : public Processor<complex_t, float> {
        using base_type = Processor<complex_t, float>;
    public:
        FM() {}

        FM(stream<complex_t>* in, double deviation) { init(in, deviation); }

        FM(stream<complex_t>* in, double deviation, double samplerate) { init(in, deviation, samplerate); }

        
        virtual void init(stream<complex_t>* in, double deviation) {
            _invDeviation = 1.0 / deviation;
            base_type::init(in);
        }

        virtual void init(stream<complex_t>* in, double deviation, double samplerate) {
            init(in, math::freqToOmega(deviation, samplerate));
        }

        void setDeviation(double deviation) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _invDeviation = 1.0 / deviation;
        }

        void setDeviation(double deviation, double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _invDeviation = 1.0 / math::freqToOmega(deviation, samplerate);
        }

        inline int process(int count, complex_t* in, float* out) {
            for (int i = 0; i < count; i++) {
                float cphase = in[i].phase();
                out[i] = math::normPhaseDiff(cphase - phase) * _invDeviation;
                phase = cphase;
            }
            return count;
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            phase = 0.0f;
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
        float _invDeviation;
        float phase = 0.0f;
    };
}