#pragma once
#include "../processor.h"

namespace dsp::digital {
    class BinarySlicer : public Processor<float, uint8_t> {
        using base_type = Processor<float, uint8_t>;
    public:
        BinarySlicer() {}

        BinarySlicer(stream<float> *in) { base_type::init(in); }

        static inline int process(int count, const float* in, uint8_t* out) {
            // TODO: Switch to volk
            for (int i = 0; i < count; i++) {
                out[i] = in[i] > 0.0f;
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
    };
}