#pragma once
#include "../operator.h"

namespace dsp::convert {
    class LRToStereo : public Operator<float, float, stereo_t> {
        using base_type = Operator<float, float, stereo_t>;
    public:
        LRToStereo() {}

        LRToStereo(stream<float>* l, stream<float>* r) { init(l, r); }

        void init(stream<float>* l, stream<float>* r) { base_type::init(l, r); }

        void setInputs(stream<float>* l, stream<float>* r) { base_type::setInputs(l, r); }

        void setInputL(stream<float>* l) { base_type::setInputA(l); }

        void setInputR(stream<float>* r) { base_type::setInputB(r); }
        
        static inline int process(int count, const float* l, const float* r, stereo_t* out) {
            volk_32f_x2_interleave_32fc((lv_32fc_t*)out, l, r, count);
            return count;
        }

        int run() {
            int a_count = base_type::_a->read();
            if (a_count < 0) { return -1; }
            int b_count = base_type::_b->read();
            if (b_count < 0) { return -1; }
            if (a_count != b_count) {
                base_type::_a->flush();
                base_type::_b->flush();
                return 0;
            }

            process(a_count, base_type::_a->readBuf, base_type::_b->readBuf, base_type::out.writeBuf);

            base_type::_a->flush();
            base_type::_b->flush();
            if (!base_type::out.swap(a_count)) { return -1; }
            return a_count;
        }
    };
}