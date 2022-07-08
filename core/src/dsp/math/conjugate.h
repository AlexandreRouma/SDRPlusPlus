#pragma once
#include "../processor.h"

namespace dsp::math {
    class Conjugate : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        Conjugate() {}

        Conjugate(stream<complex_t>* in) { base_type::init(in); }

        inline static int process(int count, const complex_t* in, complex_t* out) {
            volk_32fc_conjugate_32fc((lv_32fc_t*)out, (lv_32fc_t*)in, count);
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
    };
}