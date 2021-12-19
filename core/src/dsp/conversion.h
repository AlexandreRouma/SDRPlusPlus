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
            generic_block<ComplexToStereo>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<ComplexToStereo>::_block_init);
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
            generic_block<ComplexToReal>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<ComplexToReal>::_block_init);
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
            generic_block<ComplexToImag>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<ComplexToImag>::_block_init);
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
            if (!out.swap(count)) { return -1; }
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
            if (!generic_block<RealToComplex>::_block_init) { return; }
            generic_block<RealToComplex>::stop();
            delete[] nullBuffer;
            generic_block<RealToComplex>::_block_init = false;
        }

        void init(stream<float>* in) {
            _in = in;
            nullBuffer = new float[STREAM_BUFFER_SIZE];
            memset(nullBuffer, 0, STREAM_BUFFER_SIZE * sizeof(float));
            generic_block<RealToComplex>::registerInput(_in);
            generic_block<RealToComplex>::registerOutput(&out);
            generic_block<RealToComplex>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<RealToComplex>::_block_init);
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

    class Int16CToComplex : public generic_block<Int16CToComplex> {
    public:
        Int16CToComplex() {}

        Int16CToComplex(stream<int16_t>* in) { init(in); }

        void init(stream<int16_t>* in) {
            _in = in;
            generic_block<Int16CToComplex>::registerInput(_in);
            generic_block<Int16CToComplex>::registerOutput(&out);
            generic_block<Int16CToComplex>::_block_init = true;
        }

        void setInput(stream<int16_t>* in) {
            assert(generic_block<Int16CToComplex>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Int16CToComplex>::ctrlMtx);
            generic_block<Int16CToComplex>::tempStop();
            generic_block<Int16CToComplex>::unregisterInput(_in);
            _in = in;
            generic_block<Int16CToComplex>::registerInput(_in);
            generic_block<Int16CToComplex>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_16i_s32f_convert_32f((float*)out.writeBuf, _in->readBuf, 32768.0f, count * 2);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        stream<int16_t>* _in;
    };

    class ComplexToInt16C : public generic_block<ComplexToInt16C> {
    public:
        ComplexToInt16C() {}

        ComplexToInt16C(stream<complex_t>* in) { init(in); }

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<ComplexToInt16C>::registerInput(_in);
            generic_block<ComplexToInt16C>::registerOutput(&out);
            generic_block<ComplexToInt16C>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<ComplexToInt16C>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<ComplexToInt16C>::ctrlMtx);
            generic_block<ComplexToInt16C>::tempStop();
            generic_block<ComplexToInt16C>::unregisterInput(_in);
            _in = in;
            generic_block<ComplexToInt16C>::registerInput(_in);
            generic_block<ComplexToInt16C>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32f_s32f_convert_16i(out.writeBuf, (float*)_in->readBuf, 32768.0f, count * 2);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<int16_t> out;

    private:
        stream<complex_t>* _in;
    };

    class Int16ToFloat : public generic_block<Int16ToFloat> {
    public:
        Int16ToFloat() {}

        Int16ToFloat(stream<int16_t>* in) { init(in); }

        void init(stream<int16_t>* in) {
            _in = in;
            generic_block<Int16ToFloat>::registerInput(_in);
            generic_block<Int16ToFloat>::registerOutput(&out);
            generic_block<Int16ToFloat>::_block_init = true;
        }

        void setInput(stream<int16_t>* in) {
            assert(generic_block<Int16ToFloat>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Int16ToFloat>::ctrlMtx);
            generic_block<Int16ToFloat>::tempStop();
            generic_block<Int16ToFloat>::unregisterInput(_in);
            _in = in;
            generic_block<Int16ToFloat>::registerInput(_in);
            generic_block<Int16ToFloat>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_16i_s32f_convert_32f(out.writeBuf, _in->readBuf, 32768.0f, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        stream<int16_t>* _in;
    };

    class FloatToInt16 : public generic_block<FloatToInt16> {
    public:
        FloatToInt16() {}

        FloatToInt16(stream<float>* in) { init(in); }

        void init(stream<float>* in) {
            _in = in;
            generic_block<FloatToInt16>::registerInput(_in);
            generic_block<FloatToInt16>::registerOutput(&out);
            generic_block<FloatToInt16>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<FloatToInt16>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FloatToInt16>::ctrlMtx);
            generic_block<FloatToInt16>::tempStop();
            generic_block<FloatToInt16>::unregisterInput(_in);
            _in = in;
            generic_block<FloatToInt16>::registerInput(_in);
            generic_block<FloatToInt16>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32f_s32f_convert_16i(out.writeBuf, _in->readBuf, 32768.0f, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<int16_t> out;

    private:
        stream<float>* _in;
    };
}