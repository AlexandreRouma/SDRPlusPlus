#pragma once
#include <dsp/processor.h>
#include <dsp/loop/phase_control_loop.h>
#include <dsp/taps/windowed_sinc.h>
#include <dsp/multirate/polyphase_bank.h>
#include <dsp/math/step.h>

namespace dsp::ofdm {
    class CyclicTimeSync : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t> ;
    public:
        CyclicTimeSync() {}

        CyclicTimeSync(stream<complex_t>* in, int fftSize, int cpSize, double usefulSymbolTime, double samplerate,
                        double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) {
            init(in, fftSize, cpSize, usefulSymbolTime, samplerate, omegaGain, muGain, omegaRelLimit, interpPhaseCount, interpTapCount);
        }

        ~CyclicTimeSync() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            dsp::multirate::freePolyphaseBank(interpBank);
            buffer::free(corrSampCache);
            buffer::free(corrProdCache);
            buffer::free(buffer);
        }

        void init(stream<complex_t>* in, int fftSize, int cpSize, double usefulSymbolTime, double samplerate,
                    double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) {
            // Save parameters
            this->fftSize = fftSize;
            this->cpSize = cpSize;
            period = fftSize + cpSize;
            
            // Compute the interpolator settings
            omega = (usefulSymbolTime * samplerate) / (double)fftSize;
            this->omegaGain = omegaGain;
            this->muGain = muGain;
            this->omegaRelLimit = omegaRelLimit;
            this->interpPhaseCount = interpPhaseCount;
            this->interpTapCount = interpTapCount;

            // Compute the correlator AGC settings
            // TODO: Compute it using he FFT and CP sizes
            this->corrAgcRate = 1e-4;
            corrAgcInvRate = 1.0f - corrAgcRate;

            // Initialize the control loop
            pcl.init(muGain, omegaGain, 0.0, 0.0, 1.0, omega, omega * (1.0 - omegaRelLimit), omega * (1.0 + omegaRelLimit));

            // Generate the interpolation taps
            generateInterpTaps();

            // Allocate the buffers
            corrSampCache = buffer::alloc<complex_t>(fftSize);
            corrProdCache = buffer::alloc<complex_t>(cpSize);
            buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + interpTapCount);
            bufStart = &buffer[interpTapCount - 1];

            // Clear the buffers
            buffer::clear(corrSampCache, fftSize);
            buffer::clear(corrProdCache, cpSize);
            buffer::clear(buffer, interpTapCount - 1);
        
            base_type::init(in);
        }


        void setOmegaGain(double omegaGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            this->omegaGain = omegaGain;
            pcl.setCoefficients(muGain, omegaGain);
        }

        void setMuGain(double muGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            this->muGain = muGain;
            pcl.setCoefficients(muGain, omegaGain);
        }

        void setOmegaRelLimit(double omegaRelLimit) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            this->omegaRelLimit = omegaRelLimit;
            pcl.setFreqLimits(omega * (1.0 - omegaRelLimit), omega * (1.0 + omegaRelLimit));
        }

        void setInterpParams(int interpPhaseCount, int interpTapCount) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            this->interpPhaseCount = interpPhaseCount;
            this->interpTapCount = interpTapCount;
            dsp::multirate::freePolyphaseBank(interpBank);
            buffer::free(buffer);
            generateInterpTaps();
            buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + interpTapCount);
            bufStart = &buffer[interpTapCount - 1];
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            offset = 0;
            pcl.phase = 0.0f;
            pcl.freq = omega;
            // TODO: The rest
            base_type::tempStart();
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            // Copy data to work buffer
            memcpy(bufStart, base_type::_in->readBuf, count * sizeof(complex_t));

            // Process all samples
            while (offset < count) {
                // Get the cache slots
                complex_t* sampSlot = &corrSampCache[corrSampCacheId++];
                complex_t* prodSlot = &corrProdCache[corrProdCacheId++];
                corrSampCacheId %= fftSize;
                corrProdCacheId %= cpSize;

                // Compute the interpolated sample
                complex_t sample;
                int phase = std::clamp<int>(floorf(pcl.phase * (float)interpPhaseCount), 0, interpPhaseCount - 1);
                volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&sample, (lv_32fc_t*)&buffer[offset], interpBank.phases[phase], interpTapCount);

                // Write the sample to the output
                if (outCount >= cpSize) {
                    out.writeBuf[outCount - cpSize] = sample;
                }
                
                // Send out a symbol when it's fully received
                if ((++outCount) >= fftSize+cpSize) {
                    if (!out.swap(outCount)) { break; }
                    outCount = 0;
                }

                // Run autocorrelation
                complex_t prod = sample.conj()*(*sampSlot);
                corr += prod;
                corr -= *prodSlot;

                // Write back the new sample and product value to the cache
                *sampSlot = sample;
                *prodSlot = prod;

                // Compute the correlation level
                float corrLvl = corr.amplitude();

                // Detect peak in autocorrelation (TODO: level check maybe not needed now that corrPeak is reset to corrLvl)
                if (corrLvl > corrAvg && corrLvl > corrPeak) {
                    // Save the current correlation as the peak
                    corrPeak = corrLvl;

                    // Save the value of the previous correlation as the left side of the peak
                    corrPeakL = corrLast;

                    // Reset the peak distance counter
                    sincePeak = 0;
                }

                // The first sample after a peak is the right-side sample
                if (sincePeak == 1) {
                    corrPeakR = corrLvl;
                }
                else if (sincePeak == cpSize) {
                    // Start the useful symbol counter
                    sinceCp = 0;
                    
                    // Compute the fractional error (TODO: Probably very inaccurate with noise, use real slopes instead)
                    if (corrPeakL > corrPeakR) {
                        float maxSlope = corrPeakR - corrPeak;
                        float slope = corrPeak - corrPeakL;
                        fracErr = std::clamp<float>(0.5f * (1.0f + slope / maxSlope), -0.5f, 0.5f);
                    }
                    else {
                        float maxSlope = corrPeak - corrPeakL;
                        float slope = corrPeakR - corrPeak;
                        fracErr = std::clamp<float>(-0.5f * (1.0f + slope / maxSlope), -0.5f, 0.5f);
                    }
                }
                else if (sincePeak == fftSize) {
                    // Reset the peak detector
                    corrPeak = corrAvg;
                }
                // NOTE: THIS IS ONLY NEEDED FOR DAB
                // Detect a wider-than-normal distance to adapt the output counter
                else if (sincePeak == 2656) {
                    // Reset the output counter
                    outCount = 50;
                }
                
                // Last sample of useful symbol
                if (sinceCp == fftSize) {
                    // If the fractional error is valid, run closed-loop
                    float err = 0.0f;
                    if (!std::isnan(fracErr)) {
                        // Compute the measured period using the distance to the last symbol
                        float measuredPeriod = (float)sinceLastSym - fracErr;
                        // NOTE: THIS IS ONLY NEEDED FOR DAB
                        if (measuredPeriod > 3828.0f) {
                            // Null symbol
                            err = measuredPeriod - (2552.0f+2656.0f);
                        }
                        else {
                            // Regular symbol
                            err = measuredPeriod - period;
                        }

                        err = std::clamp<float>(err, -10.0f, 10.0f);

                        // Run the control loop in closed-loop mode
                        pcl.advance(err);
                    }
                    else {
                        // Otherwise, run open-loop
                        pcl.advancePhase();
                    }
                    
                    // printf("%d\n", outCount);

                    // Nudge the symbol window if it's too out of sync
                    if (outCount > 100) {
                        // TODO: MOVE THE LAST SAMPLES OR THE SYMBOL WILL BE CORRUPTED!
                        outCount = 50;
                        flog::debug("NUDGE!");
                    }

                    // Reset the period counter
                    sinceLastSym = 0;
                }
                else {
                    // Run the control loop in open-loop mode
                    pcl.advancePhase();
                }

                // Update the offset and phase
                float delta = floorf(pcl.phase);
                offset += delta;
                pcl.phase -= delta;

                // Update the last correlation level
                corrLast = corrLvl;

                // Update correlation AGC
                corrAvg = corrAvg*corrAgcInvRate + corrLvl*corrAgcRate;

                // Increment the distance counters (TODO: Check if they happen at the right point, eg. after being reset to zero)
                sincePeak++;
                sinceLastSym++;
                sinceCp++;
            }

            // Prepare offset for next buffer of samples
            offset -= count;

            // Update delay buffer
            memmove(buffer, &buffer[count], (interpTapCount - 1) * sizeof(complex_t));

            // Swap if some data was generated
            base_type::_in->flush();
            return count;
        }

    protected:
        void generateInterpTaps() {
            double bw = 0.5 / (double)interpPhaseCount;
            dsp::tap<float> lp = dsp::taps::windowedSinc<float>(interpPhaseCount * interpTapCount, dsp::math::hzToRads(bw, 1.0), dsp::window::nuttall, interpPhaseCount);
            interpBank = dsp::multirate::buildPolyphaseBank<float>(interpPhaseCount, lp);
            taps::free(lp);
        }

        // OFDM Configuration
        int fftSize;
        int cpSize;
        float period;

        // Interpolator
        dsp::multirate::PolyphaseBank<float> interpBank;
        int interpPhaseCount;
        int interpTapCount;
        int offset = 0;
        complex_t* buffer = NULL;
        complex_t* bufStart;
        
        // Control loop
        loop::PhaseControlLoop<float, false> pcl;
        double omega;
        double omegaGain;
        double muGain;
        double omegaRelLimit;
        float fracErr = 0.0f;
        
        // Autocorrelator
        complex_t corr = {0.0f, 0.0f};
        complex_t* corrSampCache = NULL;
        complex_t* corrProdCache = NULL;
        int corrSampCacheId = 0;
        int corrProdCacheId = 0;
        float corrAgcRate;
        float corrAgcInvRate;
        float corrAvg = 0;
        float corrLast = 0;
        float corrPeakR = 0;
        float corrPeak = 0;
        float corrPeakL = 0;

        // Peak detection
        int sincePeak = 0;
        int sinceLastSym = 0;
        int sinceCp = 0;
        
        // Other shit to categorize
        int outCount = 0;
    };
};