#pragma once
#include <dsp/block.h>
#include <dsp/math.h>

namespace dsp {
    class SineSource : public generic_block<SineSource> {
    public:
        SineSource() {}

        SineSource(int blockSize, float sampleRate, float freq) { init(blockSize, sampleRate, freq); }

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
            generic_block<SineSource>::_block_init = true;
        }

        void setBlockSize(int blockSize) {
            assert(generic_block<SineSource>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<SineSource>::ctrlMtx);
            generic_block<SineSource>::tempStop();
            _blockSize = blockSize;
            generic_block<SineSource>::tempStart();
        }

        int getBlockSize() {
            assert(generic_block<SineSource>::_block_init);
            return _blockSize;
        }

        void setSampleRate(float sampleRate) {
            assert(generic_block<SineSource>::_block_init);
            _sampleRate = sampleRate;
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
        }

        float getSampleRate() {
            assert(generic_block<SineSource>::_block_init);
            return _sampleRate;
        }

        void setFrequency(float freq) {
            assert(generic_block<SineSource>::_block_init);
            _freq = freq;
            phaseDelta = lv_cmake(std::cos((_freq / _sampleRate) * 2.0f * FL_M_PI), std::sin((_freq / _sampleRate) * 2.0f * FL_M_PI));
        }

        float getFrequency() {
            assert(generic_block<SineSource>::_block_init);
            return _freq;
        }

        int run() {
            volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)out.writeBuf, zeroPhase, phaseDelta, &phase, _blockSize);
            if (!out.swap(_blockSize)) { return -1; }
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

    template <class T>
    class HandlerSource : public generic_block<HandlerSource<T>> {
    public:
        HandlerSource() {}

        HandlerSource(int (*handler)(T* data, void* ctx), void* ctx) { init(handler, ctx); }

        void init(int (*handler)(T* data, void* ctx), void* ctx) {
            _handler = handler;
            _ctx = ctx;
            generic_block<HandlerSource<T>>::registerOutput(&out);
            generic_block<HandlerSource<T>>::_block_init = true;
        }

        void setHandler(int (*handler)(T* data, void* ctx), void* ctx) {
            assert(generic_block<HandlerSource<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<HandlerSource<T>>::ctrlMtx);
            generic_block<HandlerSource<T>>::tempStop();
            _handler = handler;
            _ctx = ctx;
            generic_block<HandlerSource<T>>::tempStart();
        }

        int run() {
            int count = _handler(out.writeBuf, _ctx);
            if (count < 0) { return -1; }
            out.swap(count);
            return count;
        }

        stream<T> out;

    private:
        int (*_handler)(T* data, void* ctx);
        void* _ctx;
    };
}