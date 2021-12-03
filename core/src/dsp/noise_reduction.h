#pragma once
#include <dsp/block.h>
#include <dsp/utils/window_functions.h>
#include <fftw3.h>

#define NR_TAP_COUNT    4096

namespace dsp {
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
}