#pragma once
#include <dsp/processor.h>
#include <dsp/math/fast_atan2.h>
#include <dsp/math/hz_to_rads.h>
#include <dsp/math/normalize_phase.h>

namespace dsp::demod {
    class Amplitude : public Processor<complex_t, float> {
        using base_type = Processor<complex_t, float>;
    public:
        Amplitude() {}

        Amplitude(stream<complex_t>* in, double deviation) { init(in, deviation); }

        Amplitude(stream<complex_t>* in, double deviation, double samplerate) { init(in, deviation, samplerate); }

        
        virtual void init(stream<complex_t>* in, double deviation) {
            _invDeviation = 1.0 / deviation;
            base_type::init(in);
        }

        virtual void init(stream<complex_t>* in, double deviation, double samplerate) {
            init(in, math::hzToRads(deviation, samplerate));
        }

        void setDeviation(double deviation) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _invDeviation = 1.0 / deviation;
        }

        void setDeviation(double deviation, double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _invDeviation = 1.0 / math::hzToRads(deviation, samplerate);
        }

        inline int process(int count, complex_t* in, float* out) {
            volk_32fc_magnitude_32f(out, (lv_32fc_t*)in, count);
            volk_32f_s32f_multiply_32f(out, out, -1.0f, count);
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