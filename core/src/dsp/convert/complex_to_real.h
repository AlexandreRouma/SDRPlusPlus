#pragma once
#include "../processor.h"

namespace dsp::convert {
    class ComplexToReal : public Processor<complex_t, float> {
        using base_type = Processor<complex_t, float>;
    public:
        ComplexToReal() {}

        ComplexToReal(stream<complex_t>* in) { init(in); }

        void init(stream<complex_t>* in) { base_type::init(in); }

        inline static int process(int count, const complex_t* in, float* out) {
            volk_32fc_deinterleave_real_32f(out, (lv_32fc_t*)in, count);
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
    };
}