#pragma once
#include <dsp/block.h>
#include <dsp/window.h>
#include <numeric>
#include <string.h>
#include <dsp/math.h>

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
            freeTapPhases();
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

            buildTapPhases();

            buffer = (T*)volk_malloc(STREAM_BUFFER_SIZE * sizeof(T) * 2, volk_get_alignment());
            memset(buffer, 0, STREAM_BUFFER_SIZE * sizeof(T) * 2);
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
            buildTapPhases();
            generic_block<PolyphaseResampler<T>>::tempStart();
        }

        void setOutSampleRate(float outSampleRate) {
            std::lock_guard<std::mutex> lck(generic_block<PolyphaseResampler<T>>::ctrlMtx);
            generic_block<PolyphaseResampler<T>>::tempStop();
            _outSampleRate = outSampleRate;
            int _gcd = std::gcd((int)_inSampleRate, (int)_outSampleRate);
            _interp = _outSampleRate / _gcd;
            _decim = _inSampleRate / _gcd;
            buildTapPhases();
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
            //generic_block<PolyphaseResampler<T>>::tempStop();
            _window = window;
            volk_free(taps);
            tapCount = window->getTapCount();
            taps = (float*)volk_malloc(tapCount * sizeof(float), volk_get_alignment());
            window->createTaps(taps, tapCount, _interp);
            buildTapPhases();
            //generic_block<PolyphaseResampler<T>>::tempStart();
        }

        int calcOutSize(int in) {
            return (in * _interp) / _decim;
        }

        virtual int run() override {
            int count = _in->read();
            if (count < 0) {
                return -1;
            }

            generic_block<PolyphaseResampler<T>>::ctrlMtx.lock();

            int outCount = calcOutSize(count);

            memcpy(&buffer[tapsPerPhase], _in->readBuf, count * sizeof(T));
            _in->flush();

            // Write to output
            int outIndex = 0;
            int _interp_m_1 = _interp - 1;
            if constexpr (std::is_same_v<T, float>) {
                for (int i = 0; outIndex < outCount; i += _decim) {
                    int phase = i % _interp;
                    volk_32f_x2_dot_prod_32f(&out.writeBuf[outIndex], &buffer[i / _interp], tapPhases[phase], tapsPerPhase);
                    outIndex++;
                }
            }
            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                for (int i = 0; outIndex < outCount; i += _decim) {
                    int phase = i % _interp;
                    volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&out.writeBuf[outIndex], (lv_32fc_t*)&buffer[(i / _interp)], tapPhases[phase], tapsPerPhase);
                    outIndex++;
                }
            }
            if (!out.swap(outCount)) { return -1; }

            memmove(buffer, &buffer[count], tapsPerPhase * sizeof(T));

            generic_block<PolyphaseResampler<T>>::ctrlMtx.unlock();

            return count;
        }

        stream<T> out;

    private:
        void buildTapPhases(){
            if(!taps){
                return;
            }

            if(!tapPhases.empty()){
                freeTapPhases();
            }

            int phases = _interp;
            tapsPerPhase = (tapCount+phases-1)/phases; //Integer division ceiling

            bufStart = &buffer[tapsPerPhase];

            for(int i = 0; i < phases; i++){
                tapPhases.push_back((float*)volk_malloc(tapsPerPhase * sizeof(float), volk_get_alignment()));
            }

            int currentTap = 0;
            for(int tap = 0; tap < tapsPerPhase; tap++) {
                for (int phase = 0; phase < phases; phase++) {
                    if(currentTap < tapCount) {
                        tapPhases[(_interp - 1) - phase][tap] = taps[currentTap++];
                    }
                    else{
                        tapPhases[(_interp - 1) - phase][tap] = 0;
                    }
                }
            }
        }

        void freeTapPhases(){
            for(auto & tapPhase : tapPhases){
                volk_free(tapPhase);
            }
            tapPhases.clear();
        }

        stream<T>* _in;

        dsp::filter_window::generic_window* _window;

        T* bufStart;
        T* buffer;
        int tapCount;
        int _interp, _decim;
        float _inSampleRate, _outSampleRate;
        float* taps;

        int tapsPerPhase;
        std::vector<float*> tapPhases;

    };

    class PowerDecimator : public generic_block<PowerDecimator> {
    public:
        PowerDecimator() {}

        PowerDecimator(stream<complex_t>* in, int power, int tapCount) { init(in, power, tapCount); }

        void init(stream<complex_t>* in, int power, int tapCount) {
            _in = in;
            _power = power;
            _tapCount = tapCount;

            // Allocate buffers
            for (int i = 0; i < _power; i++) {
                buffers[i] = new complex_t[STREAM_BUFFER_SIZE / (i+1)];
                bufferStart[i] = &buffers[i][_tapCount - 1];
            }

            // Create taps
            genHalfbandTaps();

            generic_block<PowerDecimator>::registerInput(_in);
            generic_block<PowerDecimator>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<PowerDecimator>::ctrlMtx);
            generic_block<PowerDecimator>::tempStop();
            generic_block<PowerDecimator>::unregisterInput(_in);
            _in = in;
            generic_block<PowerDecimator>::registerInput(_in);
            generic_block<PowerDecimator>::tempStart();
        }

        void setPower(unsigned int power) {
            std::lock_guard<std::mutex> lck(generic_block<PowerDecimator>::ctrlMtx);
            generic_block<PowerDecimator>::tempStop();
            _power = power;
            generic_block<PowerDecimator>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            
            if (!out.swap(count)) { return -1; }
            return count; 
        }

        stream<complex_t> out;

    private:
        void genHalfbandTaps() {
            if (taps != NULL) { delete[] taps; }
            taps = new float[_tapCount];

            // Using Blackman-harris windows
            int half = _tapCount / 2;
            for (int i = 0; i < _tapCount; i++) {
                taps[i] = sinc((FL_M_PI / 2.0f) * (i-half)) * blackmanHarrisWin(i, _tapCount - 1);
                printf("%f\n", taps[i]);
            }
        }

        inline float sinc(float x) {
            return ((x == 0) ? 1.0f : (sinf(x)/x)) / FL_M_PI;
        }

        inline float blackmanHarrisWin(float n, float N) {
            return 0.35875f - (0.48829f*cosf(2.0f*FL_M_PI*(n/N))) + (0.14128f*cosf(4.0f*FL_M_PI*(n/N))) - (0.01168f*cosf(6.0f*FL_M_PI*(n/N)));
        }

        int _power = 0;
        int _tapCount = 31;
        stream<complex_t>* _in;

        // Buffer lists, sets max decimation to 2^32
        complex_t* buffers[32];
        complex_t* bufferStart[32];

        float* taps = NULL;

    };
}