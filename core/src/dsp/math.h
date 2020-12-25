#pragma once
#include <dsp/block.h>
#include <volk/volk.h>

namespace dsp {
    template <class T>
    class Add : public generic_block<Add<T>> {
    public:
        Add() {}

        Add(stream<T>* a, stream<T>* b) { init(a, b); }

        ~Add() { generic_block<Add>::stop(); }

        void init(stream<T>* a, stream<T>* b) {
            _a = a;
            _b = b;
            generic_block<Add>::registerInput(a);
            generic_block<Add>::registerInput(b);
            generic_block<Add>::registerOutput(&out);
        }

        int run() {
            a_count = _a->read();
            if (a_count < 0) { return -1; }
            b_count = _b->read();
            if (b_count < 0) { return -1; }
            if (a_count != b_count) {
                _a->flush();
                _b->flush();
                return 0;
            }

            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                volk_32fc_x2_add_32fc(out.writeBuf, _a->readBuf, _b->readBuf, a_count);
            }
            else {
                volk_32f_x2_add_32f(out.writeBuf, _a->readBuf, _b->readBuf, a_count);
            }

            _a->flush();
            _b->flush();
            if (!out.swap(a_count)) { return -1; }
            return a_count;
        }

        stream<T> out;

    private:
        int a_count, b_count;
        stream<T>* _a;
        stream<T>* _b;

    };

    template <class T>
    class Multiply : public generic_block<Multiply<T>> {
    public:
        Multiply() {}

        Multiply(stream<T>* a, stream<T>* b) { init(a, b); }

        ~Multiply() { generic_block<Multiply>::stop(); }

        void init(stream<T>* a, stream<T>* b) {
            _a = a;
            _b = b;
            generic_block<Multiply>::registerInput(a);
            generic_block<Multiply>::registerInput(b);
            generic_block<Multiply>::registerOutput(&out);
        }

        int run() {
            a_count = _a->read();
            if (a_count < 0) { return -1; }
            b_count = _b->read();
            if (b_count < 0) { return -1; }
            if (a_count != b_count) {
                _a->flush();
                _b->flush();
                return 0;
            }

            if constexpr (std::is_same_v<T, complex_t>) {
                volk_32fc_x2_multiply_32fc(out.writeBuf, _a->readBuf, _b->readBuf, a_count);
            }
            else {
                volk_32f_x2_multiply_32f(out.writeBuf, _a->readBuf, _b->readBuf, a_count);
            }

            _a->flush();
            _b->flush();
            if (!out.swap(a_count)) { return -1; }
            return a_count;
        }

        stream<T> out;

    private:
        int a_count, b_count;
        stream<T>* _a;
        stream<T>* _b;

    };
}