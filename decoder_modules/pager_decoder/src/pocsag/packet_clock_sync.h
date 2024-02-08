#pragma once
#include <dsp/stream.h>
#include <dsp/buffer/reshaper.h>
#include <dsp/multirate/rational_resampler.h>
#include <dsp/sink/handler_sink.h>
#include <dsp/demod/quadrature.h>
#include <dsp/clock_recovery/mm.h>
#include <dsp/taps/root_raised_cosine.h>
#include <dsp/correction/dc_blocker.h>
#include <dsp/loop/fast_agc.h>
#include <dsp/digital/binary_slicer.h>
#include <dsp/routing/doubler.h>
#include <utils/flog.h>
#include <fftw3.h>
#include <dsp/math/conjugate.h>
#include <dsp/math/normalize_phase.h>

namespace dsp {
    class PacketClockSync : public dsp::Processor<float, float> {
        using base_type = dsp::Processor<float, float>;
    public:
        PacketClockSync() {}
        PacketClockSync(dsp::stream<float>* in, float* pattern, int patternLen, int frameLen, float sampsPerSym, float threshold = 0.4f) { init(in, pattern, patternLen, frameLen, sampsPerSym, threshold); }

        // TODO: Free in destroyer

        void init(dsp::stream<float>* in, float* pattern, int patternLen, int frameLen, float sampsPerSym, float threshold = 0.4f) {
            // Compute the required FFT size and associated delay length
            fftSize = 512;// TODO: Find smallest power of 2 that fits patternLen
            delayLen = fftSize - 1;
            
            // Allocate buffers
            buffer = dsp::buffer::alloc<float>(STREAM_BUFFER_SIZE+delayLen);
            bufferStart = &buffer[delayLen];
            this->pattern = dsp::buffer::alloc<float>(patternLen);
            patternFFT = dsp::buffer::alloc<complex_t>(fftSize);
            patternFFTAmps = dsp::buffer::alloc<float>(fftSize);
            fftIn = fftwf_alloc_real(fftSize);
            fftOut = (complex_t*)fftwf_alloc_complex(fftSize);

            // Copy parameters
            memcpy(this->pattern, pattern, patternLen*sizeof(float));
            this->sampsPerSym = sampsPerSym;
            this->threshold = threshold;
            this->patternLen = patternLen;

            // Plan FFT
            plan = fftwf_plan_dft_r2c_1d(fftSize, fftIn, (fftwf_complex*)fftOut, FFTW_ESTIMATE);

            // Pre-compute pattern conjugated FFT
            // TODO: Offset the pattern to avoid it being cut off (EXTREMELY IMPORTANT)
            memcpy(fftIn, pattern, patternLen*sizeof(float));
            memset(&fftIn[patternLen], 0, (fftSize-patternLen)*sizeof(float));
            fftwf_execute(plan);
            volk_32fc_conjugate_32fc((lv_32fc_t*)patternFFT, (lv_32fc_t*)fftOut, fftSize);

            // Compute amplitudes of the pattern FFT
            volk_32fc_magnitude_32f(patternFFTAmps, (lv_32fc_t*)patternFFT, fftSize);

            // Normalize the amplitudes
            float maxAmp = 0.0f;
            for (int i = 0; i < patternLen; i++) {
                if (patternFFTAmps[i] > maxAmp) { maxAmp = patternFFTAmps[i]; }
            }
            volk_32f_s32f_multiply_32f(patternFFTAmps, patternFFTAmps, 1.0f/maxAmp, fftSize);

            // Init base
            base_type::init(in);
        }

        int process(int count, float* in, float* out) {
            // Copy to buffer
            memcpy(bufferStart, in, count * sizeof(float));

           int outCount = 0;
           bool first = true;

            for (int i = 0; i < count; i++) {
                // Measure correlation to the sync pattern
                float corr;
                volk_32f_x2_dot_prod_32f(&corr, &buffer[i], pattern, patternLen);

                // If not correlated enough, go to next sample. Otherwise continue with fine detection
                if (corr/(float)patternLen < threshold) { continue; }

                // Copy samples into FFT input (only the part where we think the pattern is located)
                // TODO: Instead, check the interval onto which correlation occurs to determine where the pattern is located (IMPORTANT)
                memcpy(fftIn, &buffer[i], patternLen*sizeof(float));
                memset(&fftIn[patternLen], 0, (fftSize-patternLen)*sizeof(float)); // TODO, figure out why we need this

                // Compute FFT
                fftwf_execute(plan);

                // Multiply with the conjugated pattern FFT to get the phase offset at each frequency
                volk_32fc_x2_multiply_32fc((lv_32fc_t*)fftOut, (lv_32fc_t*)fftOut, (lv_32fc_t*)patternFFT, fftSize);
            
                // Compute the average phase delay rate
                float last = 0;
                float rateIntegral = 0;
                for (int j = 1; j < fftSize/2; j++) {
                    // Compute instantanous rate
                    float currentPhase = fftOut[j].phase();
                    float instantRate = dsp::math::normalizePhase(currentPhase - last);
                    last = currentPhase;

                    // Compute current rate guess
                    float rateGuess = rateIntegral / (float)j;

                    // Update the rate integral as a weighted average of the current guess and measured rate depending on pattern amplitude
                    rateIntegral += patternFFTAmps[j]*instantRate + (1.0f-patternFFTAmps[j])*rateGuess;
                }
                float avgRate = 1.14f*rateIntegral/(float)(fftSize/2);

                // Compute the total offset
                float offset = (float)i - avgRate*(float)fftSize/(2.0f*FL_M_PI);

                if (first) {
                    outCount = 320;
                    memcpy(out, &buffer[(int)roundf(offset)], 320*sizeof(float));
                    first = false;
                }
                

                flog::debug("Detected: {} -> {} ({})", i, offset, avgRate);
            }

            // Move unused data
            memmove(buffer, &buffer[count], delayLen * sizeof(float));

            return outCount;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            count = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            //if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    private:
        int delayLen;
        float* buffer = NULL;
        float* bufferStart = NULL;
        float* pattern = NULL;
        int patternLen;
        bool locked;
        int fftSize;

        float threshold;

        float* fftIn = NULL;
        complex_t* fftOut = NULL;
        fftwf_plan plan;

        complex_t* patternFFT;
        float* patternFFTAmps;

        float sampsPerSym;
    };
}