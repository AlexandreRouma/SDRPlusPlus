#pragma once
#include <dsp/block.h>
#include <dsp/utils/window_functions.h>
#include <fftw3.h>

#define NR_TAP_COUNT    64

namespace dsp {
    class FMIFNoiseReduction : public generic_block<FMIFNoiseReduction> {
    public:
        FMIFNoiseReduction() {}

        FMIFNoiseReduction(stream<complex_t>* in, int tapCount) { init(in, tapCount); }

        ~FMIFNoiseReduction() {
            if (!generic_block<FMIFNoiseReduction>::_block_init) { return; }
            generic_block<FMIFNoiseReduction>::stop();
            fftwf_destroy_plan(forwardPlan);
            fftwf_destroy_plan(backwardPlan);
            fftwf_free(delay);
            fftwf_free(fft_in);
            fftwf_free(fft_window);
            fftwf_free(amp_buf);
            fftwf_free(fft_cout);
            fftwf_free(fft_fcout);
        }

        void init(stream<complex_t>* in, int tapCount) {
            _in = in;
            _tapCount = tapCount;

            delay = (complex_t*)fftwf_malloc(sizeof(complex_t)*STREAM_BUFFER_SIZE);
            fft_in = (complex_t*)fftwf_malloc(sizeof(complex_t)*_tapCount);
            fft_window = (float*)fftwf_malloc(sizeof(float)*_tapCount);
            amp_buf = (float*)fftwf_malloc(sizeof(float)*_tapCount);
            fft_cout = (complex_t*)fftwf_malloc(sizeof(complex_t)*_tapCount);
            fft_fcout = (complex_t*)fftwf_malloc(sizeof(complex_t)*_tapCount);
            delay_start = &delay[_tapCount];

            memset(delay, 0, sizeof(complex_t)*STREAM_BUFFER_SIZE);
            memset(fft_in, 0, sizeof(complex_t)*_tapCount);
            memset(amp_buf, 0, sizeof(float)*_tapCount);
            memset(fft_cout, 0, sizeof(complex_t)*_tapCount);
            memset(fft_fcout, 0, sizeof(complex_t)*_tapCount);
            
            for (int i = 0; i < _tapCount; i++) {
                fft_window[i] = window_function::blackman(i, _tapCount - 1);
            }

            forwardPlan = fftwf_plan_dft_1d(_tapCount, (fftwf_complex*)fft_in, (fftwf_complex*)fft_cout, FFTW_FORWARD, FFTW_ESTIMATE);
            backwardPlan = fftwf_plan_dft_1d(_tapCount, (fftwf_complex*)fft_cout, (fftwf_complex*)fft_fcout, FFTW_BACKWARD, FFTW_ESTIMATE);

            generic_block<FMIFNoiseReduction>::registerInput(_in);
            generic_block<FMIFNoiseReduction>::registerOutput(&out);
            generic_block<FMIFNoiseReduction>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<FMIFNoiseReduction>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FMIFNoiseReduction>::ctrlMtx);
            generic_block<FMIFNoiseReduction>::tempStop();
            generic_block<FMIFNoiseReduction>::unregisterInput(_in);
            _in = in;
            generic_block<FMIFNoiseReduction>::registerInput(_in);
            generic_block<FMIFNoiseReduction>::tempStart();
        }

        void setLevel(float level) {
            _level = powf(10.0f, level / 10.0f);
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Bypass
            if (!bypass) {
                memcpy(out.writeBuf, _in->readBuf, count * sizeof(complex_t));
                _in->flush();
                if (!out.swap(count)) { return -1; }
                return count;
            }

            // Write to delay buffer
            memcpy(delay_start, _in->readBuf, count * sizeof(complex_t));

            uint32_t idx = 0;
            float actLevel = 0;

            // Iterate the FFT
            for (int i = 0; i < count; i++) {
                // Apply windows
                volk_32fc_32f_multiply_32fc((lv_32fc_t*)fft_in, (lv_32fc_t*)&delay[i], fft_window, _tapCount);

                // Do forward FFT
                fftwf_execute(forwardPlan);

                // Process bins here
                volk_32fc_magnitude_32f(amp_buf, (lv_32fc_t*)fft_cout, _tapCount);
                volk_32f_index_max_32u(&idx, amp_buf, _tapCount);

                for (int j = 0; j < _tapCount; j++) {
                    if (j == idx) { continue; }
                    fft_cout[j] = {0, 0};
                }

                // Do reverse FFT and get first element
                fftwf_execute(backwardPlan);
                out.writeBuf[i] = fft_fcout[_tapCount/2];
            }

            volk_32f_s32f_multiply_32f((float*)out.writeBuf, (float*)out.writeBuf, 1.0f/(float)_tapCount, count * 2);

            // Copy last values to delay
            memmove(delay, &delay[count], _tapCount * sizeof(complex_t));

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }
        
        bool bypass = true;
        stream<complex_t> out;

        float _level = 0.0f;

    private:
        stream<complex_t>* _in;
        fftwf_plan forwardPlan;
        fftwf_plan backwardPlan;
        complex_t* delay;
        complex_t* fft_in;
        float* fft_window;
        float* amp_buf;
        complex_t* delay_start;
        complex_t* fft_cout;
        complex_t* fft_fcout;

        int _tapCount;

    };

    class FFTNoiseReduction : public generic_block<FFTNoiseReduction> {
    public:
        FFTNoiseReduction() {}

        FFTNoiseReduction(stream<float>* in) { init(in); }

        ~FFTNoiseReduction() {
            if (!generic_block<FFTNoiseReduction>::_block_init) { return; }
            generic_block<FFTNoiseReduction>::stop();
            fftwf_destroy_plan(forwardPlan);
            fftwf_destroy_plan(backwardPlan);
            fftwf_free(delay);
            fftwf_free(fft_in);
            fftwf_free(fft_window);
            fftwf_free(amp_buf);
            fftwf_free(fft_cout);
            fftwf_free(fft_fout);
        }

        void init(stream<float>* in) {
            _in = in;

            delay = (float*)fftwf_malloc(sizeof(float)*STREAM_BUFFER_SIZE);
            fft_in = (float*)fftwf_malloc(sizeof(float)*NR_TAP_COUNT);
            fft_window = (float*)fftwf_malloc(sizeof(float)*NR_TAP_COUNT);
            amp_buf = (float*)fftwf_malloc(sizeof(float)*NR_TAP_COUNT);
            fft_cout = (complex_t*)fftwf_malloc(sizeof(complex_t)*NR_TAP_COUNT);
            fft_fout = (float*)fftwf_malloc(sizeof(float)*NR_TAP_COUNT);
            delay_start = &delay[NR_TAP_COUNT];

            memset(delay, 0, sizeof(float)*STREAM_BUFFER_SIZE);
            memset(fft_in, 0, sizeof(float)*NR_TAP_COUNT);
            memset(amp_buf, 0, sizeof(float)*NR_TAP_COUNT);
            memset(fft_cout, 0, sizeof(complex_t)*NR_TAP_COUNT);
            memset(fft_fout, 0, sizeof(float)*NR_TAP_COUNT);
            
            for (int i = 0; i < NR_TAP_COUNT; i++) {
                fft_window[i] = window_function::blackman(i, NR_TAP_COUNT - 1);
            }

            forwardPlan = fftwf_plan_dft_r2c_1d(NR_TAP_COUNT, fft_in, (fftwf_complex*)fft_cout, FFTW_ESTIMATE);
            backwardPlan = fftwf_plan_dft_c2r_1d(NR_TAP_COUNT, (fftwf_complex*)fft_cout, fft_fout, FFTW_ESTIMATE);

            generic_block<FFTNoiseReduction>::registerInput(_in);
            generic_block<FFTNoiseReduction>::registerOutput(&out);
            generic_block<FFTNoiseReduction>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<FFTNoiseReduction>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FFTNoiseReduction>::ctrlMtx);
            generic_block<FFTNoiseReduction>::tempStop();
            generic_block<FFTNoiseReduction>::unregisterInput(_in);
            _in = in;
            generic_block<FFTNoiseReduction>::registerInput(_in);
            generic_block<FFTNoiseReduction>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Bypass
            if (!bypass) {
                memcpy(out.writeBuf, _in->readBuf, count * sizeof(float));
                _in->flush();
                if (!out.swap(count)) { return -1; }
                return count;
            }

            // Write to delay buffer
            memcpy(delay_start, _in->readBuf, count * sizeof(float));

            // Iterate the FFT
            for (int i = 0; i < count; i++) {
                // Apply windows
                volk_32f_x2_multiply_32f(fft_in, &delay[i], fft_window, NR_TAP_COUNT);

                // Do forward FFT
                fftwf_execute(forwardPlan);

                // Process bins here
                volk_32fc_magnitude_32f(amp_buf, (lv_32fc_t*)fft_cout, NR_TAP_COUNT/2);
                for (int j = 1; j < NR_TAP_COUNT/2; j++) {
                    if (log10f(amp_buf[0]) < level) {
                        fft_cout[j] = {0, 0};
                    }
                }

                // Do reverse FFT and get first element
                fftwf_execute(backwardPlan);
                out.writeBuf[i] = fft_fout[NR_TAP_COUNT/2];
            }

            volk_32f_s32f_multiply_32f(out.writeBuf, out.writeBuf, 1.0f/(float)NR_TAP_COUNT, count);

            // Copy last values to delay
            memmove(delay, &delay[count], NR_TAP_COUNT * sizeof(float));

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }
        
        bool bypass = true;
        stream<float> out;

        float level = 0.0f;

    private:
        stream<float>* _in;
        fftwf_plan forwardPlan;
        fftwf_plan backwardPlan;
        float* delay;
        float* fft_in;
        float* fft_window;
        float* amp_buf;
        float* delay_start;
        complex_t* fft_cout;
        float* fft_fout;

    };

    class NoiseBlanker : public generic_block<NoiseBlanker> {
    public:
        NoiseBlanker() {}

        NoiseBlanker(stream<complex_t>* in, float attack, float decay, float threshold, float level, float sampleRate) { init(in, attack, decay, threshold, level, sampleRate); }

        ~NoiseBlanker() {
            if (!generic_block<NoiseBlanker>::_block_init) { return; }
            generic_block<NoiseBlanker>::stop();
            volk_free(ampBuf);
        }

        void init(stream<complex_t>* in, float attack, float decay, float threshold, float level, float sampleRate) {
            _in = in;
            _attack = attack;
            _decay = decay;
            _threshold = powf(10.0f, threshold / 10.0f);
            _level = level;
            _sampleRate = sampleRate;

            _inv_attack = 1.0f - _attack;
            _inv_decay = 1.0f - _decay;

            ampBuf = (float*)volk_malloc(STREAM_BUFFER_SIZE*sizeof(float), volk_get_alignment());

            generic_block<NoiseBlanker>::registerInput(_in);
            generic_block<NoiseBlanker>::registerOutput(&out);
            generic_block<NoiseBlanker>::_block_init = true;
        }

        void setAttack(float attack) {
            _attack = attack;
            _inv_attack = 1.0f - _attack;
        }

        void setDecay(float decay) {
            _decay = decay;
            _inv_decay = 1.0f - _decay;
        }

        void setThreshold(float threshold) {
            _threshold = powf(10.0f, threshold / 10.0f);
            spdlog::warn("Threshold {0}", _threshold);
        }

        void setLevel(float level) {
            _level = level;
        }

        void setSampleRate(float sampleRate) {
            _sampleRate = sampleRate;
            // TODO: Change parameters if the algo needs it
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<NoiseBlanker>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<NoiseBlanker>::ctrlMtx);
            generic_block<NoiseBlanker>::tempStop();
            generic_block<NoiseBlanker>::unregisterInput(_in);
            _in = in;
            generic_block<NoiseBlanker>::registerInput(_in);
            generic_block<NoiseBlanker>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Get amplitudes
            volk_32fc_magnitude_32f(ampBuf, (lv_32fc_t*)_in->readBuf, count);

            // Apply filtering and threshold
            float val;
            for (int i = 0; i < count; i++) {
                // Filter using attack/threshold methode
                val = ampBuf[i];
                if (val > lastValue) {
                    lastValue = (_inv_attack*lastValue) + (_attack*val);
                }
                else {
                    lastValue = (_inv_decay*lastValue) + (_decay*val);
                }

                // Apply threshold and invert
                if (lastValue > _threshold) {
                    ampBuf[i] = _threshold / (lastValue * _level);
                    if (ampBuf[i] == 0) {
                        spdlog::warn("WTF???");
                    }
                }
                else {
                    ampBuf[i] = 1.0f;
                }
            }

            // Multiply
            volk_32fc_32f_multiply_32fc((lv_32fc_t*)out.writeBuf, (lv_32fc_t*)_in->readBuf, ampBuf, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        float* ampBuf;

        float _attack;
        float _decay;
        float _inv_attack;
        float _inv_decay;
        float _threshold;
        float _level;
        float _sampleRate;

        float lastValue = 0.0f;
        
        stream<complex_t>* _in;

    };
}