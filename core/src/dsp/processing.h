#pragma once
#include <dsp/block.h>
#include <volk/volk.h>
#include <spdlog/spdlog.h>
#include <string.h>

namespace dsp {
    template <class T>
    class FrequencyXlator : public generic_block<FrequencyXlator<T>> {
    public:
        FrequencyXlator() {}

        FrequencyXlator(stream<complex_t>* in, float sampleRate, float freq) { init(in, sampleRate, freq); }

        ~FrequencyXlator() {
            generic_block<FrequencyXlator<T>>::stop();
        }

        void init(stream<complex_t>* in, float sampleRate, float freq) {
            _in = in;
            _sampleRate = sampleRate;
            _freq = freq;
            phase = lv_cmake(1.0f, 0.0f);
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
            generic_block<FrequencyXlator<T>>::registerInput(_in);
            generic_block<FrequencyXlator<T>>::registerOutput(&out);
        }

        void setInputSize(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<FrequencyXlator<T>>::ctrlMtx);
            generic_block<FrequencyXlator<T>>::tempStop();
            generic_block<FrequencyXlator<T>>::unregisterInput(_in);
            _in = in;
            generic_block<FrequencyXlator<T>>::registerInput(_in);
            generic_block<FrequencyXlator<T>>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            // No need to restart
            _sampleRate = sampleRate;
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
        }

        float getSampleRate() {
            return _sampleRate;
        }

        void setFrequency(float freq) {
            // No need to restart
            _freq = freq;
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
        }

        float getFrequency() {
            return _freq;
        }

        int run() {
            count = _in->read();
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
        int count;
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

        ~AGC() { generic_block<AGC>::stop(); }

        void init(stream<float>* in, float fallRate, float sampleRate) {
            _in = in;
            _sampleRate = sampleRate;
            _fallRate = fallRate;
            _CorrectedFallRate = _fallRate / _sampleRate;
            generic_block<AGC>::registerInput(_in);
            generic_block<AGC>::registerOutput(&out);
        }

        void setInput(stream<float>* in) {
            std::lock_guard<std::mutex> lck(generic_block<AGC>::ctrlMtx);
            generic_block<AGC>::tempStop();
            generic_block<AGC>::unregisterInput(_in);
            _in = in;
            generic_block<AGC>::registerInput(_in);
            generic_block<AGC>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            std::lock_guard<std::mutex> lck(generic_block<AGC>::ctrlMtx);
            _sampleRate = sampleRate;
            _CorrectedFallRate = _fallRate / _sampleRate;
        }

        void setFallRate(float fallRate) {
            std::lock_guard<std::mutex> lck(generic_block<AGC>::ctrlMtx);
            _fallRate = fallRate;
            _CorrectedFallRate = _fallRate / _sampleRate;
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            level = pow(10, ((10.0f * log10f(level)) - (_CorrectedFallRate * count)) / 10.0f);

            for (int i = 0; i < count; i++) {
                if (_in->readBuf[i] > level) { level = _in->readBuf[i]; }
            }

            volk_32f_s32f_multiply_32f(out.writeBuf, _in->readBuf, 1.0f / level, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        int count;
        float level = 0.0f;
        float _fallRate;
        float _CorrectedFallRate;
        float _sampleRate;
        stream<float>* _in;

    };

    template <class T>
    class Volume : public generic_block<Volume<T>> {
    public:
        Volume() {}

        Volume(stream<T>* in, float volume) { init(in, volume); }

        ~Volume() { generic_block<Volume<T>>::stop(); }

        void init(stream<T>* in, float volume) {
            _in = in;
            _volume = volume;
            generic_block<Volume<T>>::registerInput(_in);
            generic_block<Volume<T>>::registerOutput(&out);
        }

        void setInputSize(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<Volume<T>>::ctrlMtx);
            generic_block<Volume<T>>::tempStop();
            generic_block<Volume<T>>::unregisterInput(_in);
            _in = in;
            generic_block<Volume<T>>::registerInput(_in);
            generic_block<Volume<T>>::tempStart();
        }

        void setVolume(float volume) {
            _volume = volume;
            level = powf(_volume, 2);
        }

        float getVolume() {
            return _volume;
        }

        void setMuted(bool muted) {
            _muted = muted;
        }

        bool getMuted() {
            return _muted;
        }

        int run() {
            count = _in->read();
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
        int count;
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
            generic_block<Squelch>::stop();
            delete[] normBuffer;
        }

        void init(stream<complex_t>* in, float level) {
            _in = in;
            _level = level;
            normBuffer = new float[STREAM_BUFFER_SIZE];
            generic_block<Squelch>::registerInput(_in);
            generic_block<Squelch>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<Squelch>::ctrlMtx);
            generic_block<Squelch>::tempStop();
            generic_block<Squelch>::unregisterInput(_in);
            _in = in;
            generic_block<Squelch>::registerInput(_in);
            generic_block<Squelch>::tempStart();
        }

        void setLevel(float level) {
            _level = level;
        }

        float getLevel() {
            return _level;
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            float sum = 0.0f;
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
        int count;
        float* normBuffer;
        float _level = -50.0f;
        stream<complex_t>* _in;

    };
}