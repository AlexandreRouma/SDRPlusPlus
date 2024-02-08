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
            this->frameLen = frameLen;

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
            for (int i = 0; i < fftSize/2; i++) {
                if (patternFFTAmps[i] > maxAmp) { maxAmp = patternFFTAmps[i]; }
            }
            volk_32f_s32f_multiply_32f(patternFFTAmps, patternFFTAmps, 1.0f/maxAmp, fftSize);

            // Initialize the phase control loop
            float omegaRelLimit = 0.05;
            pcl.init(1, 10e-4, 0.0, 0.0, 1.0, sampsPerSym, sampsPerSym * (1.0 - omegaRelLimit), sampsPerSym * (1.0 + omegaRelLimit));
            generateInterpTaps();

            // Init base
            base_type::init(in);
        }

        int process(int count, float* in, float* out) {
            // Copy to buffer
            memcpy(bufferStart, in, count * sizeof(float));

            int outCount = 0;

            for (int i = 0; i < count;) {
                // Run clock recovery if needed
                while (toRead) {
                    // Interpolate symbol
                    float symbol;
                    int phase = std::clamp<int>(floorf(pcl.phase * (float)interpPhaseCount), 0, interpPhaseCount - 1);
                    volk_32f_x2_dot_prod_32f(&symbol, &buffer[offsetInt], interpBank.phases[phase], interpTapCount);
                    out[outCount++] = symbol;

                    // Compute symbol phase error
                    float error = (math::step(lastSymbol) * symbol) - (lastSymbol * math::step(symbol));
                    lastSymbol = symbol;

                    // Clamp symbol phase error
                    if (error > 1.0f) { error = 1.0f; }
                    if (error < -1.0f) { error = -1.0f; }

                    // Advance symbol offset and phase
                    pcl.advance(error);
                    float delta = floorf(pcl.phase);
                    offsetInt += delta;
                    i = offsetInt;
                    pcl.phase -= delta;

                    // Decrement read counter
                    toRead--;

                    if (offsetInt >= count) {
                        offsetInt -= count;
                        break;
                    }
                }


                // Measure correlation to the sync pattern
                float corr;
                volk_32f_x2_dot_prod_32f(&corr, &buffer[i], pattern, patternLen);

                // If not correlated enough, go to next sample. Otherwise continue with fine detection
                if (corr/(float)patternLen < threshold) {
                    i++;
                    continue;
                }

                // Copy samples into FFT input (only the part where we think the pattern is located)
                // TODO: Instead, check the interval onto which correlation occurs to determine where the pattern is located (IMPORTANT)
                memcpy(fftIn, &buffer[i], patternLen*sizeof(float));

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
                flog::debug("Detected: {} -> {}", i, offset);

                // Initialize clock recovery
                offsetInt = floorf(offset) - 3; // TODO: Will be negative sometimes, has to be taken into account
                pcl.phase = offset - (float)floorf(offset);
                pcl.freq = sampsPerSym;

                // Start reading symbols
                toRead = frameLen;
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
            if (count) {
                if (!base_type::out.swap(count)) { return -1; }
            }
            return count;
        }

    private:
        void generateInterpTaps() {
            double bw = 0.5 / (double)interpPhaseCount;
            dsp::tap<float> lp = dsp::taps::windowedSinc<float>(interpPhaseCount * interpTapCount, dsp::math::hzToRads(bw, 1.0), dsp::window::nuttall, interpPhaseCount);
            interpBank = dsp::multirate::buildPolyphaseBank<float>(interpPhaseCount, lp);
            taps::free(lp);
        }

        int delayLen;
        float* buffer = NULL;
        float* bufferStart = NULL;
        float* pattern = NULL;
        int patternLen;
        bool locked;
        int fftSize;
        int frameLen;
        float threshold;

        float* fftIn = NULL;
        complex_t* fftOut = NULL;
        fftwf_plan plan;

        complex_t* patternFFT;
        float* patternFFTAmps;

        float sampsPerSym;
        int toRead = 0;

        loop::PhaseControlLoop<float, false> pcl;
        dsp::multirate::PolyphaseBank<float> interpBank;
        int interpTapCount = 8;
        int interpPhaseCount = 128;
        float lastSymbol = 0.0f;
        int offsetInt;
    };
}