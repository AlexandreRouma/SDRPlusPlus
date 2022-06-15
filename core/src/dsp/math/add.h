#pragma once
#include "../operator.h"

namespace dsp::math {
    template <class T>
    class Add : public Operator<T, T, T> {
        using base_type = Operator<T, T, T>;
    public:
        Add() {}

        Add(stream<T>* a, stream<T>* b) { base_type::init(a, b); }

        static inline int process(int count, const T* a, const T*b, T* out) {
            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                volk_32f_x2_add_32f((float*)out, (float*)a, (float*)b, count * 2);
            }
            else {
                volk_32f_x2_add_32f(out, a, b, count);
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
