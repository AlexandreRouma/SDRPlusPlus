#pragma once
#include <dsp/block.h>
#include <dsp/window.h>
#include <numeric>
#include <string.h>

namespace dsp {
    template <class T>
    class PolyphaseResampler : public generic_block<PolyphaseResampler<T>> {
    public:
        PolyphaseResampler() {}

        PolyphaseResampler(stream<T>* in, dsp::filter_window::generic_window* window, float inSampleRate, float outSampleRate) { init(in, window, inSampleRate, outSampleRate); }

        ~PolyphaseResampler() {
            generic_block<PolyphaseResampler<T>>::stop();
            volk_free(buffer);
            volk_free(taps);
        }

        void init(stream<T>* in, dsp::filter_window::generic_window* window, float inSampleRate, float outSampleRate) {
            _in = in;
            _window = window;
            _inSampleRate = inSampleRate;
            _outSampleRate = outSampleRate;

            int _gcd = std::gcd((int)_inSampleRate, (int)_outSampleRate);
            _interp = _outSampleRate / _gcd;
            _decim = _inSampleRate / _gcd;

            tapCount = _window->getTapCount();
            taps = (float*)volk_malloc(tapCount * sizeof(float), volk_get_alignment());
            _window->createTaps(taps, tapCount, _interp);

            buffer = (T*)volk_malloc(STREAM_BUFFER_SIZE * sizeof(T) * 2, volk_get_alignment());
            memset(buffer, 0, STREAM_BUFFER_SIZE * sizeof(T) * 2);
            bufStart = &buffer[tapCount];
            generic_block<PolyphaseResampler<T>>::registerInput(_in);
            generic_block<PolyphaseResampler<T>>::registerOutput(&out);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<PolyphaseResampler<T>>::ctrlMtx);
            generic_block<PolyphaseResampler<T>>::tempStop();
            generic_block<PolyphaseResampler<T>>::unregisterInput(_in);
            _in = in;
            generic_block<PolyphaseResampler<T>>::registerInput(_in);
            generic_block<PolyphaseResampler<T>>::tempStart();
        }

        void setInSampleRate(float inSampleRate) {
            std::lock_guard<std::mutex> lck(generic_block<PolyphaseResampler<T>>::ctrlMtx);
            generic_block<PolyphaseResampler<T>>::tempStop();
            _inSampleRate = inSampleRate;
            int _gcd = std::gcd((int)_inSampleRate, (int)_outSampleRate);
            _interp = _outSampleRate / _gcd;
            _decim = _inSampleRate / _gcd;
            generic_block<PolyphaseResampler<T>>::tempStart();
        }

        void setOutSampleRate(float outSampleRate) {
            std::lock_guard<std::mutex> lck(generic_block<PolyphaseResampler<T>>::ctrlMtx);
            generic_block<PolyphaseResampler<T>>::tempStop();
            _outSampleRate = outSampleRate;
            int _gcd = std::gcd((int)_inSampleRate, (int)_outSampleRate);
            _interp = _outSampleRate / _gcd;
            _decim = _inSampleRate / _gcd;
            generic_block<PolyphaseResampler<T>>::tempStart();
        }

        int getInterpolation() {
            return _interp;
        }

        int getDecimation() {
            return _decim;
        }

        void updateWindow(dsp::filter_window::generic_window* window) {
            std::lock_guard<std::mutex> lck(generic_block<PolyphaseResampler<T>>::ctrlMtx);
            generic_block<PolyphaseResampler<T>>::tempStop();
            _window = window;
            volk_free(taps);
            tapCount = window->getTapCount();
            taps = (float*)volk_malloc(tapCount * sizeof(float), volk_get_alignment());
            window->createTaps(taps, tapCount, _interp);
            bufStart = &buffer[tapCount];
            generic_block<PolyphaseResampler<T>>::tempStart();
        }

        int calcOutSize(int in) {
            return (in * _interp) / _decim;
        }

        virtual int run() override {
            count = _in->read();
            if (count < 0) {
                return -1;
            }

            int outCount = calcOutSize(count);

            memcpy(&buffer[tapCount], _in->readBuf, count * sizeof(T));
            _in->flush();

            // Write to output
            int outIndex = 0;
            if constexpr (std::is_same_v<T, float>) {
                for (int i = 0; outIndex < outCount; i += _decim) {
                    out.writeBuf[outIndex] = 0;
                    for (int j = i % _interp; j < tapCount; j += _interp) {
                        out.writeBuf[outIndex] += buffer[((i - j) / _interp) + tapCount] * taps[j];
                    }
                    outIndex++;
                }
            }
            if constexpr (std::is_same_v<T, complex_t>) {
                for (int i = 0; outIndex < outCount; i += _decim) {
                    out.writeBuf[outIndex].i = 0;
                    out.writeBuf[outIndex].q = 0;
                    for (int j = i % _interp; j < tapCount; j += _interp) {
                        out.writeBuf[outIndex].i += buffer[((i - j) / _interp) + tapCount].i * taps[j];
                        out.writeBuf[outIndex].q += buffer[((i - j) / _interp) + tapCount].q * taps[j];
                    }
                    outIndex++;
                }
            }
            if (!out.swap(outCount)) { return -1; }

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
        int _interp, _decim;
        float _inSampleRate, _outSampleRate;
        float* taps;

    };
}