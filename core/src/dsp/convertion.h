#pragma once
#include <dsp/block.h>

namespace dsp {
    class ComplexToStereo : public generic_block<ComplexToStereo> {
    public:
        ComplexToStereo() {}

        ComplexToStereo(stream<complex_t>* in) { init(in); }

        ~ComplexToStereo() { generic_block<ComplexToStereo>::stop(); }

        static_assert(sizeof(complex_t) == sizeof(stereo_t));

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<ComplexToStereo>::registerInput(_in);
            generic_block<ComplexToStereo>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<ComplexToStereo>::ctrlMtx);
            generic_block<ComplexToStereo>::tempStop();
            generic_block<ComplexToStereo>::unregisterInput(_in);
            _in = in;
            generic_block<ComplexToStereo>::registerInput(_in);
            generic_block<ComplexToStereo>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            memcpy(out.writeBuf, _in->readBuf, count * sizeof(complex_t));

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<stereo_t> out;

    private:
        float avg;
        int count;
        stream<complex_t>* _in;

    };

    class ComplexToReal : public generic_block<ComplexToReal> {
    public:
        ComplexToReal() {}

        ComplexToReal(stream<complex_t>* in) { init(in); }

        ~ComplexToReal() { generic_block<ComplexToReal>::stop(); }

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<ComplexToReal>::registerInput(_in);
            generic_block<ComplexToReal>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<ComplexToReal>::ctrlMtx);
            generic_block<ComplexToReal>::tempStop();
            generic_block<ComplexToReal>::unregisterInput(_in);
            _in = in;
            generic_block<ComplexToReal>::registerInput(_in);
            generic_block<ComplexToReal>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_deinterleave_real_32f(out.writeBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        float avg;
        int count;
        stream<complex_t>* _in;

    };

    class ComplexToImag : public generic_block<ComplexToImag> {
    public:
        ComplexToImag() {}

        ComplexToImag(stream<complex_t>* in) { init(in); }

        ~ComplexToImag() { generic_block<ComplexToImag>::stop(); }

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<ComplexToImag>::registerInput(_in);
            generic_block<ComplexToImag>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<ComplexToImag>::ctrlMtx);
            generic_block<ComplexToImag>::tempStop();
            generic_block<ComplexToImag>::unregisterInput(_in);
            _in = in;
            generic_block<ComplexToImag>::registerInput(_in);
            generic_block<ComplexToImag>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_deinterleave_imag_32f(out.writeBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();
            if(!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        float avg;
        int count;
        stream<complex_t>* _in;

    };
}