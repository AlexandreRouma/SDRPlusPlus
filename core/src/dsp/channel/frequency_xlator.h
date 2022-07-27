#pragma once
#include "../processor.h"
#include "../math/hz_to_rads.h"

namespace dsp::channel {
    class FrequencyXlator : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        FrequencyXlator() {}

        FrequencyXlator(stream<complex_t>* in, double offset) { init(in, offset); }

        FrequencyXlator(stream<complex_t>* in, double offset, double samplerate) { init(in, offset, samplerate); }

        void init(stream<complex_t>* in, double offset) {
            phase = lv_cmake(1.0f, 0.0f);
            phaseDelta = lv_cmake(cos(offset), sin(offset));
            base_type::init(in);
        }

        void init(stream<complex_t>* in, double offset, double samplerate) {
            init(in, math::hzToRads(offset, samplerate));
        }

        void setOffset(double offset) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            phaseDelta = lv_cmake(cos(offset), sin(offset));
        }

        void setOffset(double offset, double samplerate) {
            setOffset(math::hzToRads(offset, samplerate));
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            tempStop();
            phase = lv_cmake(1.0f, 0.0f);
            tempStart();
        }

        inline int process(int count, const complex_t* in, complex_t* out) {
            volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)out, (lv_32fc_t*)in, phaseDelta, &phase, count);
            return count;
        }

        virtual int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        lv_32fc_t phase;
        lv_32fc_t phaseDelta;
    };
}