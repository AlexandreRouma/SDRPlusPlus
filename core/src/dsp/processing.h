#pragma once
#include <dsp/block.h>

namespace dsp {
    class FrequencyXlator : public generic_block<FrequencyXlator> {
    public:
        FrequencyXlator() {}

        FrequencyXlator(stream<complex_t>* in, float sampleRate, float freq) { init(in, sampleRate, freq); }

        ~FrequencyXlator() { generic_block<FrequencyXlator>::stop(); }

        void init(stream<complex_t>* in, float sampleRate, float freq) {
            _in = in;
            _sampleRate = sampleRate;
            _freq = freq;
            phase = lv_cmake(1.0f, 0.0f);
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
            generic_block<FrequencyXlator>::registerOutput(&out);
        }

        void setInputSize(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<FrequencyXlator>::ctrlMtx);
            generic_block<FrequencyXlator>::tempStop();
            _in = in;
            generic_block<FrequencyXlator>::tempStart();
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

            if (out.aquire() < 0) { return -1; }

            volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)out.data, (lv_32fc_t*)_in->data, phaseDelta, &phase, count);

            _in->flush();
            out.write(count);
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
}