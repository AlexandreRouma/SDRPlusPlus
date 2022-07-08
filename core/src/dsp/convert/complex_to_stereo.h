#pragma once
#include "../processor.h"

namespace dsp::convert {
    class ComplexToStereo : public Processor<complex_t, stereo_t> {
        using base_type = Processor<complex_t, stereo_t>;
    public:
        ComplexToStereo() {}

        ComplexToStereo(stream<complex_t>* in) { init(in); }

        void init(stream<complex_t>* in) { base_type::init(in); }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            memcpy(base_type::out.writeBuf, base_type::_in->readBuf, count * sizeof(complex_t));

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }
    };
}