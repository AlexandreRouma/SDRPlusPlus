#pragma once
#include <dsp/block.h>

namespace dsp {
    class ComplexToStereo : public generic_block<ComplexToStereo> {
    public:
        ComplexToStereo() {}

        ComplexToStereo(stream<complex_t>* in) { init(in); }

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
            int count = _in->read();
            if (count < 0) { return -1; }

            memcpy(out.writeBuf, _in->readBuf, count * sizeof(complex_t));

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<stereo_t> out;

    private:
        stream<complex_t>* _in;

    };

    class ComplexToReal : public generic_block<ComplexToReal> {
    public:
        ComplexToReal() {}

        ComplexToReal(stream<complex_t>* in) { init(in); }

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
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_deinterleave_real_32f(out.writeBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        stream<complex_t>* _in;

    };

    class ComplexToImag : public generic_block<ComplexToImag> {
    public:
        ComplexToImag() {}

        ComplexToImag(stream<complex_t>* in) { init(in); }

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
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_deinterleave_imag_32f(out.writeBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();
            if(!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        stream<complex_t>* _in;

    };


    class RealToComplex : public generic_block<RealToComplex> {
    public:
        RealToComplex() {}

        RealToComplex(stream<float>* in) { init(in); }

        ~RealToComplex() {
            generic_block<RealToComplex>::stop();
            delete[] nullBuffer;
        }

        void init(stream<float>* in) {
            _in = in;
            nullBuffer = new float[STREAM_BUFFER_SIZE];
            memset(nullBuffer, 0, STREAM_BUFFER_SIZE * sizeof(float));
            generic_block<RealToComplex>::registerInput(_in);
            generic_block<RealToComplex>::registerOutput(&out);
        }

        void setInput(stream<float>* in) {
            std::lock_guard<std::mutex> lck(generic_block<RealToComplex>::ctrlMtx);
            generic_block<RealToComplex>::tempStop();
            generic_block<RealToComplex>::unregisterInput(_in);
            _in = in;
            generic_block<RealToComplex>::registerInput(_in);
            generic_block<RealToComplex>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32f_x2_interleave_32fc((lv_32fc_t*)out.writeBuf, _in->readBuf, nullBuffer, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        float* nullBuffer;
        stream<float>* _in;

    };
}