#pragma once
#include <dsp/block.h>

namespace dsp {
    class SineSource : public generic_block<SineSource> {
    public:
        SineSource() {}

        SineSource(int blockSize, float sampleRate, float freq) { init(blockSize, sampleRate, freq); }

        ~SineSource() { generic_block<SineSource>::stop(); }

        void init(int blockSize, float sampleRate, float freq) {
            _blockSize = blockSize;
            _sampleRate = sampleRate;
            _freq = freq;
            zeroPhase = (lv_32fc_t*)volk_malloc(STREAM_BUFFER_SIZE * sizeof(lv_32fc_t), volk_get_alignment());
            for (int i = 0; i < STREAM_BUFFER_SIZE; i++) {
                zeroPhase[i] = lv_cmake(1.0f, 0.0f);
            }
            phase = lv_cmake(1.0f, 0.0f);
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
            generic_block<SineSource>::registerOutput(&out);
        }

        void setBlockSize(int blockSize) {
            std::lock_guard<std::mutex> lck(generic_block<SineSource>::ctrlMtx);
            generic_block<SineSource>::tempStop();
            _blockSize = blockSize;
            generic_block<SineSource>::tempStart();
        }

        int getBlockSize() {
            return _blockSize;
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
            if (out.aquire() < 0) { return -1; }
            volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)out.data, zeroPhase, phaseDelta, &phase, _blockSize);
            out.write(_blockSize);
            return _blockSize;
        }

        stream<complex_t> out;

    private:
        int _blockSize;
        float _sampleRate;
        float _freq;
        lv_32fc_t phaseDelta;
        lv_32fc_t phase;
        lv_32fc_t* zeroPhase;

    };
}