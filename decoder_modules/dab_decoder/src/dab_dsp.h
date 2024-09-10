#pragma once
#include <dsp/processor.h>
#include <utils/flog.h>
#include <fftw3.h>
#include "dab_phase_sym.h"

namespace dab {
    class CyclicSync : public dsp::Processor<dsp::complex_t, dsp::complex_t> {
        using base_type = dsp::Processor<dsp::complex_t, dsp::complex_t>;
    public:
        CyclicSync() {}

        // TODO: The default AGC rate is probably way too fast, plot out the avgCorr to see how much it moves
        CyclicSync(dsp::stream<dsp::complex_t>* in, double symbolLength, double cyclicPrefixLength, double samplerate, float agcRate = 1e-3) { init(in, symbolLength, cyclicPrefixLength, samplerate, agcRate); }

        void init(dsp::stream<dsp::complex_t>* in, double symbolLength, double cyclicPrefixLength, double samplerate, float agcRate = 1e-3) {
            // Computer the number of samples for the symbol and its cyclic prefix
            symbolSamps = round(samplerate * symbolLength);
            prefixSamps = round(samplerate * cyclicPrefixLength);

            // Allocate and clear the delay buffer
            delayBuf = dsp::buffer::alloc<dsp::complex_t>(STREAM_BUFFER_SIZE + 64000);
            dsp::buffer::clear(delayBuf, symbolSamps);

            // Allocate and clear the history buffer
            histBuf = dsp::buffer::alloc<dsp::complex_t>(prefixSamps);
            dsp::buffer::clear(histBuf, prefixSamps);

            // Compute the delay input addresses
            delayBufInput = &delayBuf[symbolSamps];

            // Compute the correlation AGC configuration
            this->agcRate = agcRate;
            agcRateInv = 1.0f - agcRate;
            
            base_type::init(in);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            
            base_type::tempStart();
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            // Copy the data into the normal delay buffer
            memcpy(delayBufInput, base_type::_in->readBuf, count * sizeof(dsp::complex_t));

            // Flush the input stream
            base_type::_in->flush();

            // Do cross-correlation
            for (int i = 0; i < count; i++) {
                // Get the current history slot
                dsp::complex_t* slot = &histBuf[histId++];

                // Wrap around the history slot index (TODO: Check that the history buffer's length is correct)
                histId %= prefixSamps;

                // Kick out last value from the correlation
                corr -= *slot;

                // Save input value and compute the new prodct
                dsp::complex_t val = delayBuf[i];
                dsp::complex_t prod = val.conj()*delayBuf[i+symbolSamps];

                // Add the new value to the correlation
                *slot = prod;

                // Add the new value to the history buffer
                corr += prod;

                // Compute sample amplitude
                float rcorr = corr.amplitude();

                // If a high enough peak is reached, reset the symbol counter
                if (rcorr > avgCorr && rcorr > peakCorr) { // Note keeping an average level might not be needed
                    peakCorr = rcorr;
                    peakLCorr = lastCorr;
                    samplesSincePeak = 0;
                }

                // If this is the sample right after the peak, save it
                if (samplesSincePeak == 1) {
                    peakRCorr = rcorr;
                }

                // Write the sample to the output
                out.writeBuf[samplesSincePeak++] = val;

                // If the end of the symbol is reached, send it off
                if (samplesSincePeak >= symbolSamps) {
                    if (!out.swap(symbolSamps)) {
                        return -1;
                    }
                    samplesSincePeak = 0;
                    peakCorr = 0;
                }

                // Update the average correlation
                lastCorr = rcorr;

                // Update the average correlation value
                avgCorr = agcRate*rcorr + agcRateInv*avgCorr;
            }

            // Move unused data
            memmove(delayBuf, &delayBuf[count], symbolSamps * sizeof(dsp::complex_t));

            return count;
        }

    protected:
        int symbolSamps;
        int prefixSamps;

        int histId = 0;
        dsp::complex_t* histBuf;

        dsp::complex_t* delayBuf;
        dsp::complex_t* delayBufInput;

        dsp::complex_t corr = { 0.0f, 0.0f };

        int samplesSincePeak = 0;
        float lastCorr = 0.0f;
        float peakCorr = 0.0f;
        float peakLCorr = 0.0f;
        float peakRCorr = 0.0f;

        // Note only required for DAB
        float avgCorr = 0.0f;
        float agcRate;
        float agcRateInv;
    };

    class FrameFreqSync : public dsp::Processor<dsp::complex_t, dsp::complex_t> {
        using base_type = dsp::Processor<dsp::complex_t, dsp::complex_t>;
    public:
        FrameFreqSync() {}

        FrameFreqSync(dsp::stream<dsp::complex_t>* in, float agcRate = 0.01f) { init(in, agcRate); }

        void init(dsp::stream<dsp::complex_t>* in, float agcRate = 0.01f) {
            // Allocate buffers
            amps = dsp::buffer::alloc<float>(2048);
            conjRef = dsp::buffer::alloc<dsp::complex_t>(2048);
            corrIn = (dsp::complex_t*)fftwf_alloc_complex(2048);
            corrOut = (dsp::complex_t*)fftwf_alloc_complex(2048);

            // Copy the phase reference
            memcpy(conjRef, DAB_PHASE_SYM_CONJ, 2048 * sizeof(dsp::complex_t));

            // Plan the FFT computation
            plan = fftwf_plan_dft_1d(2048, (fftwf_complex*)corrIn, (fftwf_complex*)corrOut, FFTW_FORWARD, FFTW_ESTIMATE);

            // Compute the correlation AGC configuration
            this->agcRate = agcRate;
            agcRateInv = 1.0f - agcRate;
            
            base_type::init(in);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            
            base_type::tempStart();
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            // Apply frequency shift
            lv_32fc_t phase = lv_cmake(1.0f, 0.0f);
            lv_32fc_t phaseDelta = lv_cmake(cos(offset), sin(offset));
#if VOLK_VERSION >= 030100
            volk_32fc_s32fc_x2_rotator2_32fc((lv_32fc_t*)_in->readBuf, (lv_32fc_t*)_in->readBuf, phaseDelta, &phase, count);
#else
            volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)_in->readBuf, (lv_32fc_t*)_in->readBuf, phaseDelta, &phase, count);
#endif

            // Compute the amplitude amplitude of all samples
            volk_32fc_magnitude_32f(amps, (lv_32fc_t*)_in->readBuf, 2048);

            // Compute the average signal level by adding up all values
            float level = 0.0f;
            volk_32f_accumulator_s32f(&level, amps, 2048);

            // Detect a frame sync condition
            if (level < avgLvl * 0.5f) {
                // Reset symbol counter
                sym = 1;

                // Update the average level
                avgLvl = agcRate*level + agcRateInv*avgLvl;

                // Flush the input stream and return
                base_type::_in->flush();
                return count;
            }

            // Update the average level
            avgLvl = agcRate*level + agcRateInv*avgLvl;

            // Handle phase reference
            if (sym == 1) {
                // Output the symbols (DEBUG ONLY)
                memcpy(corrIn, _in->readBuf, 2048 * sizeof(dsp::complex_t));
                fftwf_execute(plan);
                volk_32fc_magnitude_32f(amps, (lv_32fc_t*)corrOut, 2048);
                int outCount = 0;
                dsp::complex_t pi4 = { cos(3.1415926535*0.25), sin(3.1415926535*0.25) };
                for (int i = -767; i < 768; i++) {
                    if (!i) { continue; }
                    int cid0 = ((i-1) >= 0) ? (i-1) : 2048+(i-1);
                    int cid1 = (i >= 0) ? i : 2048+i;;
                    out.writeBuf[outCount++] = pi4 * (corrOut[cid1] * corrOut[cid0].conj()) * (1.0f/(amps[cid0]*amps[cid0]));
                }
                out.swap(outCount);

                // Multiply the samples with the conjugated phase reference signal
                volk_32fc_x2_multiply_32fc((lv_32fc_t*)corrIn, (lv_32fc_t*)_in->readBuf, (lv_32fc_t*)conjRef, 2048);
            
                // Compute the FFT of the product
                fftwf_execute(plan);

                // Compute the amplitude of the bins
                volk_32fc_magnitude_32f(amps, (lv_32fc_t*)corrOut, 2048);

                // Locate highest power bin
                uint32_t peakId;
                volk_32f_index_max_32u(&peakId, amps, 2048);

                // Obtain the value of the bins next to the peak
                float peakL = amps[(peakId + 2047) % 2048];
                float peakR = amps[(peakId + 1) % 2048];

                // Compute the integer frequency offset
                float offInt = (peakId < 1024) ? (float)peakId : ((float)peakId - 2048.0f);

                // Compute the frequency offset in rad/samp
                float off = 3.1415926535f * (offInt + ((peakR - peakL) / (peakR + peakL))) * (1.0f / 1024.0f);

                // Run control loop
                offset -= 0.1f*off;
                flog::debug("Offset: {} Hz, Error: {} Hz, Avg Level: {}", offset * (0.5f/3.1415926535f)*2.048e6, off * (0.5f/3.1415926535f)*2.048e6, avgLvl);
            }

            // Increment the symbol counter
            sym++;

            // Flush the input stream and return
            base_type::_in->flush();
            return count;
        }

    protected:
        fftwf_plan plan;

        float* amps;
        dsp::complex_t* conjRef;
        dsp::complex_t* corrIn;
        dsp::complex_t* corrOut;

        int sym;
        float offset = 0.0f;

        float avgLvl = 0.0f;
        float agcRate;
        float agcRateInv;
    };
}