#pragma once
#include "../processor.h"

namespace dsp::convert {
    class StereoToMono : public Processor<stereo_t, float> {
        using base_type = Processor<stereo_t, float>;
    public:
        StereoToMono() {}

        StereoToMono(stream<stereo_t>* in) { base_type::init(in); }
        
        inline int process(int count, const stereo_t* in, float* out) {
            for (int i = 0; i < count; i++) {
                out[i] = (in[i].l + in[i].r) / 2.0f;
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