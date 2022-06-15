#pragma once
#include "../processor.h"

namespace dsp::convert {
    class RealToComplex : public Processor<float, complex_t> {
        using base_type = Processor<float, complex_t>;
    public:
        RealToComplex() {}

        RealToComplex(stream<float>* in) { init(in); }

        ~RealToComplex() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            buffer::free(nullBuf);
        }

        void init(stream<float>* in) {
            nullBuf = buffer::alloc<float>(STREAM_BUFFER_SIZE);
            buffer::clear(nullBuf, STREAM_BUFFER_SIZE);
            base_type::init(in);
        }

        inline int process(int count, const float* in, complex_t* out) {
            volk_32f_x2_interleave_32fc((lv_32fc_t*)out, in, nullBuf, count);
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

    private:
        float* nullBuf;

    };
}