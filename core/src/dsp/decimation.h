#pragma once
#include <dsp/block.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/window.h>

namespace dsp {
    template <class T>
    class HalfDecimator : public generic_block<HalfDecimator<T>> {
    public:
        HalfDecimator() {}

        HalfDecimator(stream<T>* in, dsp::filter_window::generic_window* window) { init(in, window); }

        ~HalfDecimator() {
            if (!generic_block<HalfDecimator<T>>::_block_init) { return; }
            generic_block<HalfDecimator<T>>::stop();
            volk_free(buffer);
            volk_free(taps);
            generic_block<HalfDecimator<T>>::_block_init = false;
        }

        void init(stream<T>* in, dsp::filter_window::generic_window* window) {
            _in = in;

            tapCount = window->getTapCount();
            taps = (float*)volk_malloc(tapCount * sizeof(float), volk_get_alignment());
            window->createTaps(taps, tapCount);

            buffer = (T*)volk_malloc(STREAM_BUFFER_SIZE * sizeof(T) * 2, volk_get_alignment());
            bufStart = &buffer[tapCount];
            generic_block<HalfDecimator<T>>::registerInput(_in);
            generic_block<HalfDecimator<T>>::registerOutput(&out);
            generic_block<HalfDecimator<T>>::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(generic_block<HalfDecimator<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<HalfDecimator<T>>::ctrlMtx);
            generic_block<HalfDecimator<T>>::tempStop();
            generic_block<HalfDecimator<T>>::unregisterInput(_in);
            _in = in;
            generic_block<HalfDecimator<T>>::registerInput(_in);
            generic_block<HalfDecimator<T>>::tempStart();
        }

        void updateWindow(dsp::filter_window::generic_window* window) {
            assert(generic_block<HalfDecimator<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<HalfDecimator<T>>::ctrlMtx);
            _window = window;
            volk_free(taps);
            tapCount = window->getTapCount();
            taps = (float*)volk_malloc(tapCount * sizeof(float), volk_get_alignment());
            bufStart = &buffer[tapCount];
            window->createTaps(taps, tapCount);
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            generic_block<HalfDecimator<T>>::ctrlMtx.lock();

            memcpy(bufStart, _in->readBuf, count * sizeof(T));
            _in->flush();

            int inIndex = _inIndex;
            int outIndex = 0;
            if constexpr (std::is_same_v<T, float>) {
                while (inIndex < count) {
                    volk_32f_x2_dot_prod_32f((float*)&out.writeBuf[outIndex], (float*)&buffer[inIndex+1], taps, tapCount);
                    inIndex += 2;
                    outIndex++;
                }
            }
            if constexpr (std::is_same_v<T, complex_t>) {
                while (inIndex < count) {
                    volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&out.writeBuf[outIndex], (lv_32fc_t*)&buffer[inIndex+1], taps, tapCount);
                    inIndex += 2;
                    outIndex++;
                }
            }
            _inIndex = inIndex - count;

            if (!out.swap(outIndex)) { return -1; }

            memmove(buffer, &buffer[count], tapCount * sizeof(T));

            generic_block<HalfDecimator<T>>::ctrlMtx.unlock();

            return count;
        }

        stream<T> out;

    private:
        stream<T>* _in;

        dsp::filter_window::generic_window* _window;

        T* bufStart;
        T* buffer;
        int tapCount;
        float* taps;
        int _inIndex = 0;

    };
}