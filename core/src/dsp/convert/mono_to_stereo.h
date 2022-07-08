#pragma once
#include "../processor.h"

namespace dsp::convert {
    class MonoToStereo : public Processor<float, stereo_t> {
        using base_type = Processor<float, stereo_t>;
    public:
        MonoToStereo() {}

        MonoToStereo(stream<float>* in) { base_type::init(in); }
        
        inline static int process(int count, const float* in, stereo_t* out) {
            volk_32f_x2_interleave_32fc((lv_32fc_t*)out, in, in, count);
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