#pragma once
#include <dsp/block.h>
#include <volk/volk.h>

namespace dsp {
    template <class T>
    class Add : public generic_block<Add<T>> {
    public:
        Add() {}

        Add(stream<T>* a, stream<T>* b) { init(a, b); }

        void init(stream<T>* a, stream<T>* b) {
            _a = a;
            _b = b;
            generic_block<Add<T>>::registerInput(a);
            generic_block<Add<T>>::registerInput(b);
            generic_block<Add<T>>::registerOutput(&out);
            generic_block<Add<T>>::_block_init = true;
        }

        void setInputs(stream<T>* a, stream<T>* b) {
            assert(generic_block<Add<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Add<T>>::ctrlMtx);
            generic_block<Add<T>>::tempStop();
            generic_block<Add<T>>::unregisterInput(_a);
            generic_block<Add<T>>::unregisterInput(_b);
            _a = a;
            _b = b;
            generic_block<Add<T>>::registerInput(_a);
            generic_block<Add<T>>::registerInput(_b);
            generic_block<Add<T>>::tempStart();
        }

        void setInputA(stream<T>* a) {
            assert(generic_block<Add<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Add<T>>::ctrlMtx);
            generic_block<Add<T>>::tempStop();
            generic_block<Add<T>>::unregisterInput(_a);
            _a = a;
            generic_block<Add<T>>::registerInput(_a);
            generic_block<Add<T>>::tempStart();
        }

        void setInputB(stream<T>* b) {
            assert(generic_block<Add<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Add<T>>::ctrlMtx);
            generic_block<Add<T>>::tempStop();
            generic_block<Add<T>>::unregisterInput(_b);
            _b = b;
            generic_block<Add<T>>::registerInput(_b);
            generic_block<Add<T>>::tempStart();
        }

        int run() {
            int a_count = _a->read();
            if (a_count < 0) { return -1; }
            int b_count = _b->read();
            if (b_count < 0) { return -1; }
            if (a_count != b_count) {
                _a->flush();
                _b->flush();
                return 0;
            }

            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                // TODO: Switch this out for volk_32fc_x2_add_32fc with a check for old volk versions that don't have it (eg. Ubuntu 18.04 that has volk 1.3)
                volk_32f_x2_add_32f((float*)out.writeBuf, (float*)_a->readBuf, (float*)_b->readBuf, a_count * 2);
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
        stream<T>* _a;
        stream<T>* _b;

    };

    template <class T>
    class Subtract : public generic_block<Subtract<T>> {
    public:
        Subtract() {}

        Subtract(stream<T>* a, stream<T>* b) { init(a, b); }

        void init(stream<T>* a, stream<T>* b) {
            _a = a;
            _b = b;
            generic_block<Subtract<T>>::registerInput(a);
            generic_block<Subtract<T>>::registerInput(b);
            generic_block<Subtract<T>>::registerOutput(&out);
            generic_block<Subtract<T>>::_block_init = true;
        }

        void setInputs(stream<T>* a, stream<T>* b) {
            assert(generic_block<Subtract<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Subtract<T>>::ctrlMtx);
            generic_block<Subtract<T>>::tempStop();
            generic_block<Subtract<T>>::unregisterInput(_a);
            generic_block<Subtract<T>>::unregisterInput(_b);
            _a = a;
            _b = b;
            generic_block<Subtract<T>>::registerInput(_a);
            generic_block<Subtract<T>>::registerInput(_b);
            generic_block<Subtract<T>>::tempStart();
        }

        void setInputA(stream<T>* a) {
            assert(generic_block<Subtract<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Subtract<T>>::ctrlMtx);
            generic_block<Subtract<T>>::tempStop();
            generic_block<Subtract<T>>::unregisterInput(_a);
            _a = a;
            generic_block<Subtract<T>>::registerInput(_a);
            generic_block<Subtract<T>>::tempStart();
        }

        void setInputB(stream<T>* b) {
            assert(generic_block<Subtract<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Subtract<T>>::ctrlMtx);
            generic_block<Subtract<T>>::tempStop();
            generic_block<Subtract<T>>::unregisterInput(_b);
            _b = b;
            generic_block<Subtract<T>>::registerInput(_b);
            generic_block<Subtract<T>>::tempStart();
        }

        int run() {
            int a_count = _a->read();
            if (a_count < 0) { return -1; }
            int b_count = _b->read();
            if (b_count < 0) { return -1; }
            if (a_count != b_count) {
                _a->flush();
                _b->flush();
                return 0;
            }

            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                volk_32f_x2_subtract_32f((float*)out.writeBuf, (float*)_a->readBuf, (float*)_b->readBuf, a_count * 2);
            }
            else {
                volk_32f_x2_subtract_32f(out.writeBuf, _a->readBuf, _b->readBuf, a_count);
            }

            _a->flush();
            _b->flush();
            if (!out.swap(a_count)) { return -1; }
            return a_count;
        }

        stream<T> out;

    private:
        stream<T>* _a;
        stream<T>* _b;

    };

    template <class T>
    class Multiply : public generic_block<Multiply<T>> {
    public:
        Multiply() {}

        Multiply(stream<T>* a, stream<T>* b) { init(a, b); }

        void init(stream<T>* a, stream<T>* b) {
            _a = a;
            _b = b;
            generic_block<Multiply<T>>::registerInput(a);
            generic_block<Multiply<T>>::registerInput(b);
            generic_block<Multiply<T>>::registerOutput(&out);
            generic_block<Multiply<T>>::_block_init = true;
        }

        void setInputs(stream<T>* a, stream<T>* b) {
            assert(generic_block<Multiply<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Multiply<T>>::ctrlMtx);
            generic_block<Multiply<T>>::tempStop();
            generic_block<Multiply<T>>::unregisterInput(_a);
            generic_block<Multiply<T>>::unregisterInput(_b);
            _a = a;
            _b = b;
            generic_block<Multiply<T>>::registerInput(_a);
            generic_block<Multiply<T>>::registerInput(_b);
            generic_block<Multiply<T>>::tempStart();
        }

        void setInputA(stream<T>* a) {
            assert(generic_block<Multiply<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Multiply<T>>::ctrlMtx);
            generic_block<Multiply<T>>::tempStop();
            generic_block<Multiply<T>>::unregisterInput(_a);
            _a = a;
            generic_block<Multiply<T>>::registerInput(_a);
            generic_block<Multiply<T>>::tempStart();
        }

        void setInputB(stream<T>* b) {
            assert(generic_block<Multiply<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Multiply<T>>::ctrlMtx);
            generic_block<Multiply<T>>::tempStop();
            generic_block<Multiply<T>>::unregisterInput(_b);
            _b = b;
            generic_block<Multiply<T>>::registerInput(_b);
            generic_block<Multiply<T>>::tempStart();
        }

        int run() {
            int a_count = _a->read();
            if (a_count < 0) { return -1; }
            int b_count = _b->read();
            if (b_count < 0) { return -1; }
            if (a_count != b_count) {
                _a->flush();
                _b->flush();
                return 0;
            }

            if constexpr (std::is_same_v<T, complex_t>) {
                volk_32fc_x2_multiply_32fc((lv_32fc_t*)out.writeBuf, (lv_32fc_t*)_a->readBuf, (lv_32fc_t*)_b->readBuf, a_count);
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
        stream<T>* _a;
        stream<T>* _b;

    };
}