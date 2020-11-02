#pragma once
#include <dsp/block.h>
#include <dsp/window.h>

namespace dsp {

    template <class T>
    class FIR : public generic_block<FIR<T>> {
    public:
        FIR() {}

        FIR(stream<T>* in, dsp::filter_window::generic_window* window) { init(in, window); }

        ~FIR() {
            generic_block<FIR<T>>::stop();
            volk_free(buffer);
            volk_free(taps);
        }

        void init(stream<T>* in, dsp::filter_window::generic_window* window) {
            _in = in;

            tapCount = window->getTapCount();
            taps = (float*)volk_malloc(tapCount * sizeof(float), volk_get_alignment());
            window->createTaps(taps, tapCount);

            buffer = (T*)volk_malloc(STREAM_BUFFER_SIZE * sizeof(T) * 2, volk_get_alignment());
            bufStart = &buffer[tapCount];
            generic_block<FIR<T>>::registerInput(_in);
            generic_block<FIR<T>>::registerOutput(&out);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<FIR<T>>::ctrlMtx);
            generic_block<FIR<T>>::tempStop();
            generic_block<FIR<T>>::unregisterInput(_in);
            _in = in;
            generic_block<FIR<T>>::registerInput(_in);
            generic_block<FIR<T>>::tempStart();
        }

        void updateWindow(dsp::filter_window::generic_window* window) {
            _window = window;
            volk_free(taps);
            tapCount = window->getTapCount();
            taps = (float*)volk_malloc(tapCount * sizeof(float), volk_get_alignment());
            window->createTaps(taps, tapCount);
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            memcpy(bufStart, _in->data, count * sizeof(T));
            _in->flush();

            // Write to output
            if (out.aquire() < 0) { return -1; } 

            if constexpr (std::is_same_v<T, float>) {
                for (int i = 0; i < count; i++) {
                    volk_32f_x2_dot_prod_32f((float*)&out.data[i], (float*)&buffer[i+1], taps, tapCount);
                }
            }
            if constexpr (std::is_same_v<T, complex_t>) {
                for (int i = 0; i < count; i++) {
                    volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&out.data[i], (lv_32fc_t*)&buffer[i+1], taps, tapCount);
                }
            }

            out.write(count);

            memmove(buffer, &buffer[count], tapCount * sizeof(T));

            return count;
        }

        stream<T> out;

    private:
        int count;
        stream<T>* _in;

        dsp::filter_window::generic_window* _window;

        T* bufStart;
        T* buffer;
        int tapCount;
        float* taps;

    };

    class BFMDeemp : public generic_block<BFMDeemp> {
    public:
        BFMDeemp() {}

        BFMDeemp(stream<float>* in, float sampleRate, float tau) { init(in, sampleRate, tau); }

        ~BFMDeemp() { generic_block<BFMDeemp>::stop(); }

        void init(stream<float>* in, float sampleRate, float tau) {
            _in = in;
            _sampleRate = sampleRate;
            _tau = tau;
            float dt = 1.0f / _sampleRate;
            alpha = dt / (_tau + dt);
            generic_block<BFMDeemp>::registerInput(_in);
            generic_block<BFMDeemp>::registerOutput(&out);
        }

        void setInput(stream<float>* in) {
            std::lock_guard<std::mutex> lck(generic_block<BFMDeemp>::ctrlMtx);
            generic_block<BFMDeemp>::tempStop();
            generic_block<BFMDeemp>::unregisterInput(_in);
            _in = in;
            generic_block<BFMDeemp>::registerInput(_in);
            generic_block<BFMDeemp>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            _sampleRate = sampleRate;
            float dt = 1.0f / _sampleRate;
            alpha = dt / (_tau + dt);
        }

        void setTau(float tau) {
            _tau = tau;
            float dt = 1.0f / _sampleRate;
            alpha = dt / (_tau + dt);
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            if (bypass) {
                if (out.aquire() < 0) { return -1; } 
                _in->flush();
                out.write(count);
            }

            if (isnan(lastOut)) {
                lastOut = 0.0f;
            }
            if (out.aquire() < 0) { return -1; } 
            out.data[0] = (alpha * _in->data[0]) + ((1-alpha) * lastOut);
            for (int i = 1; i < count; i++) {
                out.data[i] = (alpha * _in->data[i]) + ((1 - alpha) * out.data[i - 1]);
            }
            lastOut = out.data[count - 1];

            _in->flush();
            out.write(count);
            return count;
        }

        bool bypass = false;

        stream<float> out;

    private:
        int count;
        float lastOut = 0.0f;
        float alpha;
        float _tau;
        float _sampleRate;
        stream<float>* _in;

    };
}