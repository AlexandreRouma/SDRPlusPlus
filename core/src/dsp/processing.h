#pragma once
#include <dsp/block.h>
#include <volk/volk.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include <stdint.h>
#include <dsp/math.h>

namespace dsp {
    template <class T>
    class FrequencyXlator : public generic_block<FrequencyXlator<T>> {
    public:
        FrequencyXlator() {}

        FrequencyXlator(stream<complex_t>* in, float sampleRate, float freq) { init(in, sampleRate, freq); }

        void init(stream<complex_t>* in, float sampleRate, float freq) {
            _in = in;
            _sampleRate = sampleRate;
            _freq = freq;
            phase = lv_cmake(1.0f, 0.0f);
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
            generic_block<FrequencyXlator<T>>::registerInput(_in);
            generic_block<FrequencyXlator<T>>::registerOutput(&out);
            generic_block<FrequencyXlator<T>>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<FrequencyXlator<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FrequencyXlator<T>>::ctrlMtx);
            generic_block<FrequencyXlator<T>>::tempStop();
            generic_block<FrequencyXlator<T>>::unregisterInput(_in);
            _in = in;
            generic_block<FrequencyXlator<T>>::registerInput(_in);
            generic_block<FrequencyXlator<T>>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            assert(generic_block<FrequencyXlator<T>>::_block_init);
            _sampleRate = sampleRate;
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
        }

        float getSampleRate() {
            assert(generic_block<FrequencyXlator<T>>::_block_init);
            return _sampleRate;
        }

        void setFrequency(float freq) {
            assert(generic_block<FrequencyXlator<T>>::_block_init);
            _freq = freq;
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
        }

        float getFrequency() {
            assert(generic_block<FrequencyXlator<T>>::_block_init);
            return _freq;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // TODO: Do float xlation
            if constexpr (std::is_same_v<T, float>) {
                spdlog::error("XLATOR NOT IMPLEMENTED FOR FLOAT");
            }
            if constexpr (std::is_same_v<T, complex_t>) {
                volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)out.writeBuf, (lv_32fc_t*)_in->readBuf, phaseDelta, &phase, count);
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        float _sampleRate;
        float _freq;
        lv_32fc_t phaseDelta;
        lv_32fc_t phase;
        stream<complex_t>* _in;

    };

    class AGC : public generic_block<AGC> {
    public:
        AGC() {}

        AGC(stream<float>* in, float fallRate, float sampleRate) { init(in, fallRate, sampleRate); }

        void init(stream<float>* in, float fallRate, float sampleRate) {
            _in = in;
            _sampleRate = sampleRate;
            _fallRate = fallRate;
            _CorrectedFallRate = _fallRate / _sampleRate;
            generic_block<AGC>::registerInput(_in);
            generic_block<AGC>::registerOutput(&out);
            generic_block<AGC>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<AGC>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<AGC>::ctrlMtx);
            generic_block<AGC>::tempStop();
            generic_block<AGC>::unregisterInput(_in);
            _in = in;
            generic_block<AGC>::registerInput(_in);
            generic_block<AGC>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            assert(generic_block<AGC>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<AGC>::ctrlMtx);
            _sampleRate = sampleRate;
            _CorrectedFallRate = _fallRate / _sampleRate;
        }

        void setFallRate(float fallRate) {
            assert(generic_block<AGC>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<AGC>::ctrlMtx);
            _fallRate = fallRate;
            _CorrectedFallRate = _fallRate / _sampleRate;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            level = pow(10, ((10.0f * log10f(level)) - (_CorrectedFallRate * count)) / 10.0f);

            if (level < 10e-14) { level = 10e-14; }

            float absVal;
            for (int i = 0; i < count; i++) {
                absVal = fabsf(_in->readBuf[i]);
                if (absVal > level) { level = absVal; }
            }


            volk_32f_s32f_multiply_32f(out.writeBuf, _in->readBuf, 1.0f / level, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        float level = 0.0f;
        float _fallRate;
        float _CorrectedFallRate;
        float _sampleRate;
        stream<float>* _in;

    };

    class ComplexAGC : public generic_block<ComplexAGC> {
    public:
        ComplexAGC() {}

        ComplexAGC(stream<complex_t>* in, float setPoint, float maxGain, float rate) { init(in, setPoint, maxGain, rate); }

        void init(stream<complex_t>* in, float setPoint, float maxGain, float rate) {
            _in = in;
            _setPoint = setPoint;
            _maxGain = maxGain;
            _rate = rate;
            generic_block<ComplexAGC>::registerInput(_in);
            generic_block<ComplexAGC>::registerOutput(&out);
            generic_block<ComplexAGC>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<ComplexAGC>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<ComplexAGC>::ctrlMtx);
            generic_block<ComplexAGC>::tempStop();
            generic_block<ComplexAGC>::unregisterInput(_in);
            _in = in;
            generic_block<ComplexAGC>::registerInput(_in);
            generic_block<ComplexAGC>::tempStart();
        }

        void setSetPoint(float setPoint) {
            assert(generic_block<ComplexAGC>::_block_init);
            _setPoint = setPoint;
        }

        void setMaxGain(float maxGain) {
            assert(generic_block<ComplexAGC>::_block_init);
            _maxGain = maxGain;
        }

        void setRate(float rate) {
            assert(generic_block<ComplexAGC>::_block_init);
            _rate = rate;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            dsp::complex_t val;
            for (int i = 0; i < count; i++) {
                val = _in->readBuf[i] * _gain;
                out.writeBuf[i] = val;
                _gain += (_setPoint - val.amplitude()) * _rate;
                if (_gain > _maxGain) { _gain = _maxGain; }
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        float _gain = 1.0f;
        float _setPoint = 1.0f;
        float _maxGain = 10e4;
        float _rate = 10e-4;
        
        stream<complex_t>* _in;

    };

    class DelayImag : public generic_block<DelayImag> {
    public:
        DelayImag() {}

        DelayImag(stream<complex_t>* in) { init(in); }

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<DelayImag>::registerInput(_in);
            generic_block<DelayImag>::registerOutput(&out);
            generic_block<DelayImag>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<DelayImag>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<DelayImag>::ctrlMtx);
            generic_block<DelayImag>::tempStop();
            generic_block<DelayImag>::unregisterInput(_in);
            _in = in;
            generic_block<DelayImag>::registerInput(_in);
            generic_block<DelayImag>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            dsp::complex_t val;
            for (int i = 0; i < count; i++) {
                val = _in->readBuf[i];
                out.writeBuf[i].re = val.re;
                out.writeBuf[i].im = lastIm;
                lastIm = val.im;
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        float lastIm = 0.0f;
        stream<complex_t>* _in;

    };



    template <class T>
    class Volume : public generic_block<Volume<T>> {
    public:
        Volume() {}

        Volume(stream<T>* in, float volume) { init(in, volume); }

        void init(stream<T>* in, float volume) {
            _in = in;
            _volume = volume;
            generic_block<Volume<T>>::registerInput(_in);
            generic_block<Volume<T>>::registerOutput(&out);
            generic_block<Volume<T>>::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(generic_block<Volume<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Volume<T>>::ctrlMtx);
            generic_block<Volume<T>>::tempStop();
            generic_block<Volume<T>>::unregisterInput(_in);
            _in = in;
            generic_block<Volume<T>>::registerInput(_in);
            generic_block<Volume<T>>::tempStart();
        }

        void setVolume(float volume) {
            assert(generic_block<Volume<T>>::_block_init);
            _volume = volume;
            level = powf(_volume, 2);
        }

        float getVolume() {
            assert(generic_block<Volume<T>>::_block_init);
            return _volume;
        }

        void setMuted(bool muted) {
            assert(generic_block<Volume<T>>::_block_init);
            _muted = muted;
        }

        bool getMuted() {
            assert(generic_block<Volume<T>>::_block_init);
            return _muted;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            if (_muted) {
                if constexpr (std::is_same_v<T, stereo_t>) {
                    memset(out.writeBuf, 0, sizeof(stereo_t) * count);
                }
                else {
                    memset(out.writeBuf, 0, sizeof(float) * count);
                }
            }
            else {
                if constexpr (std::is_same_v<T, stereo_t>) {
                    volk_32f_s32f_multiply_32f((float*)out.writeBuf, (float*)_in->readBuf, level, count * 2);
                }
                else {
                    volk_32f_s32f_multiply_32f((float*)out.writeBuf, (float*)_in->readBuf, level, count);
                }
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<T> out;

    private:
        float level = 1.0f;
        float _volume = 1.0f;
        bool _muted = false;
        stream<T>* _in;

    };

    class Squelch : public generic_block<Squelch> {
    public:
        Squelch() {}

        Squelch(stream<complex_t>* in, float level) { init(in, level); }

        ~Squelch() {
            if (!generic_block<Squelch>::_block_init) { return; }
            generic_block<Squelch>::stop();
            delete[] normBuffer;
            generic_block<Squelch>::_block_init = false;
        }

        void init(stream<complex_t>* in, float level) {
            _in = in;
            _level = level;
            normBuffer = new float[STREAM_BUFFER_SIZE];
            generic_block<Squelch>::registerInput(_in);
            generic_block<Squelch>::registerOutput(&out);
            generic_block<Squelch>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<Squelch>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Squelch>::ctrlMtx);
            generic_block<Squelch>::tempStop();
            generic_block<Squelch>::unregisterInput(_in);
            _in = in;
            generic_block<Squelch>::registerInput(_in);
            generic_block<Squelch>::tempStart();
        }

        void setLevel(float level) {
            assert(generic_block<Squelch>::_block_init);
            _level = level;
        }

        float getLevel() {
            assert(generic_block<Squelch>::_block_init);
            return _level;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            float sum;
            volk_32fc_magnitude_32f(normBuffer, (lv_32fc_t*)_in->readBuf, count);
            volk_32f_accumulator_s32f(&sum, normBuffer, count);
            sum /= (float)count;

            if (10.0f * log10f(sum) >= _level) {
                memcpy(out.writeBuf, _in->readBuf, count * sizeof(complex_t));
            }
            else {
                memset(out.writeBuf, 0, count * sizeof(complex_t));
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count; 
        }

        stream<complex_t> out;


    private:
        float* normBuffer;
        float _level = -50.0f;
        stream<complex_t>* _in;

    };

    template <class T>
    class Packer : public generic_block<Packer<T>> {
    public:
        Packer() {}

        Packer(stream<T>* in, int count) { init(in, count); }

        void init(stream<T>* in, int count) {
            _in = in;
            samples = count;
            generic_block<Packer<T>>::registerInput(_in);
            generic_block<Packer<T>>::registerOutput(&out);
            generic_block<Packer<T>>::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(generic_block<Packer<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Packer<T>>::ctrlMtx);
            generic_block<Packer<T>>::tempStop();
            generic_block<Packer<T>>::unregisterInput(_in);
            _in = in;
            generic_block<Packer<T>>::registerInput(_in);
            generic_block<Packer<T>>::tempStart();
        }

        void setSampleCount(int count) {
            assert(generic_block<Packer<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Packer<T>>::ctrlMtx);
            generic_block<Packer<T>>::tempStop();
            samples = count;
            generic_block<Packer<T>>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) {
                read = 0;
                return -1;
            }

            for (int i = 0; i < count; i++) {
                out.writeBuf[read++] = _in->readBuf[i];
                if (read >= samples) {
                    read = 0;
                    if (!out.swap(samples)) {
                        _in->flush();
                        read = 0;
                        return -1;
                    }
                }
            }

            _in->flush();
            
            return count;
        }

        stream<T> out;

    private:
        int samples = 1;
        int read = 0;
        stream<T>* _in;

    };

    class Threshold : public generic_block<Threshold> {
    public:
        Threshold() {}

        Threshold(stream<float>* in) { init(in); }

        ~Threshold() {
            if (!generic_block<Threshold>::_block_init) { return; }
            generic_block<Threshold>::stop();
            delete[] normBuffer;
            generic_block<Threshold>::_block_init = false;
        }

        void init(stream<float>* in) {
            _in = in;
            normBuffer = new float[STREAM_BUFFER_SIZE];
            generic_block<Threshold>::registerInput(_in);
            generic_block<Threshold>::registerOutput(&out);
            generic_block<Threshold>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<Threshold>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Threshold>::ctrlMtx);
            generic_block<Threshold>::tempStop();
            generic_block<Threshold>::unregisterInput(_in);
            _in = in;
            generic_block<Threshold>::registerInput(_in);
            generic_block<Threshold>::tempStart();
        }

        void setLevel(float level) {
            assert(generic_block<Threshold>::_block_init);
            _level = level;
        }

        float getLevel() {
            assert(generic_block<Threshold>::_block_init);
            return _level;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            for (int i = 0; i < count; i++) {
                out.writeBuf[i] = (_in->readBuf[i] > 0.0f);
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count; 
        }

        stream<uint8_t> out;


    private:
        float* normBuffer;
        float _level = -50.0f;
        stream<float>* _in;

    };

    class BFMPilotToStereo : public generic_block<BFMPilotToStereo> {
    public:
        BFMPilotToStereo() {}

        BFMPilotToStereo(stream<complex_t>* in) { init(in); }

        ~BFMPilotToStereo() {
            generic_block<BFMPilotToStereo>::stop();
            delete[] buffer;
        }

        void init(stream<complex_t>* in) {
            _in = in;

            buffer = new complex_t[STREAM_BUFFER_SIZE];

            generic_block<BFMPilotToStereo>::registerInput(_in);
            generic_block<BFMPilotToStereo>::registerOutput(&out);
            generic_block<BFMPilotToStereo>::_block_init = true;
        }

        void setInputs(stream<complex_t>* in) {
            assert(generic_block<BFMPilotToStereo>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<BFMPilotToStereo>::ctrlMtx);
            generic_block<BFMPilotToStereo>::tempStop();
            generic_block<BFMPilotToStereo>::unregisterInput(_in);
            _in = in;
            generic_block<BFMPilotToStereo>::registerInput(_in);
            generic_block<BFMPilotToStereo>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_x2_multiply_32fc((lv_32fc_t*)buffer, (lv_32fc_t*)_in->readBuf, (lv_32fc_t*)_in->readBuf, count);
            _in->flush();

            volk_32fc_conjugate_32fc((lv_32fc_t*)out.writeBuf, (lv_32fc_t*)buffer, count);

            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        stream<complex_t>* _in;

        complex_t* buffer;

    };
}