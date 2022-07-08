#pragma once
#include "../operator.h"

namespace dsp::math {
    template <class T>
    class Multiply : public Operator<T, T, T> {
        using base_type = Operator<T, T, T>;
    public:
        Multiply() {}

        Multiply(stream<T>* a, stream<T>* b) { base_type::init(a, b); }

        inline static int process(int count, const T* a, const T* b, T* out) {
            if constexpr (std::is_same_v<T, complex_t>) {
                volk_32fc_x2_multiply_32fc((lv_32fc_t*)out, (lv_32fc_t*)a, (lv_32fc_t*)b, count);
            }
            else {
                volk_32f_x2_multiply_32f(out, a, b, count);
            }
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