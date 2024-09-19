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

        CyclicTimeSync(stream<complex_t>* in, int fftSize, double usefulSymbolTime, double cyclicPrefixRatio, double samplerate,
                        double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) {
            init(in, fftSize, usefulSymbolTime, cyclicPrefixRatio, samplerate, omegaGain, muGain, omegaRelLimit, interpPhaseCount, interpTapCount);
        }

        ~CyclicTimeSync() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            dsp::multirate::freePolyphaseBank(interpBank);
            buffer::free(buffer);
        }

        void init(stream<complex_t>* in, int fftSize, double usefulSymbolTime, double cyclicPrefixRatio, double samplerate,
                    double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) {
            omega = 0; // TODO
            _omegaGain = omegaGain;
            _muGain = muGain;
            _omegaRelLimit = omegaRelLimit;
            _interpPhaseCount = interpPhaseCount;
            _interpTapCount = interpTapCount;

            pcl.init(_muGain, _omegaGain, 0.0, 0.0, 1.0, omega, omega * (1.0 - omegaRelLimit), omega * (1.0 + omegaRelLimit));
            generateInterpTaps();
            buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + _interpTapCount);
            bufStart = &buffer[_interpTapCount - 1];
        
            base_type::init(in);
        }

        void setOmegaGain(double omegaGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _omegaGain = omegaGain;
            pcl.setCoefficients(_muGain, _omegaGain);
        }

        void setMuGain(double muGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _muGain = muGain;
            pcl.setCoefficients(_muGain, _omegaGain);
        }

        void setOmegaRelLimit(double omegaRelLimit) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _omegaRelLimit = omegaRelLimit;
            pcl.setFreqLimits(omega * (1.0 - _omegaRelLimit), omega * (1.0 + _omegaRelLimit));
        }

        void setInterpParams(int interpPhaseCount, int interpTapCount) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _interpPhaseCount = interpPhaseCount;
            _interpTapCount = interpTapCount;
            dsp::multirate::freePolyphaseBank(interpBank);
            buffer::free(buffer);
            generateInterpTaps();
            buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + _interpTapCount);
            bufStart = &buffer[_interpTapCount - 1];
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            offset = 0;
            pcl.phase = 0.0f;
            pcl.freq = omega;
            base_type::tempStart();
        }

        inline int process(int count, const complex_t* in, complex_t* out) {
            // Copy data to work buffer
            memcpy(bufStart, in, count * sizeof(complex_t));

            // Process all samples
            int outCount = 0;
            while (offset < count) {
                // Compute the interpolated sample
                complex_t sample;
                int phase = std::clamp<int>(floorf(pcl.phase * (float)_interpPhaseCount), 0, _interpPhaseCount - 1);
                volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&sample, (lv_32fc_t*)&buffer[offset], interpBank.phases[phase], _interpTapCount);

                // Update autocorrelation

                // Compute the correlation level
                float corrLvl = corr.amplitude();

                // Detect peak in autocorrelation
                if (0/*TODO*/) {
                    // Save the current correlation as the peak
                    corrPeak = corrLvl;

                    // Save the value of the previous correlation as the left side of the peak
                    corrPeakL = corrLastLvl;

                    // Save the symbol period
                    measuredSymbolPeriod = sampCount;

                    // Reset the sample count
                    sampCount = 0;

                    // TODO: Maybe save the error to apply it at the end of the frame? (will  cause issues with the longer null symbol in DAB)
                }

                // Write the sample to the frame if within it
                if (sampCount < symbolSize) {
                    symbol[sampCount++] = sample;
                }

                // When the end of the symbol is reached
                if (sampCount == symbolSize) {
                    // Send out the symbol
                    // TODO
                }

                // TODO: limit how much the sample count can grow otherwise otherflows will trigger a false frame detection

                // Run the control loop
                //pcl.advance(error); // TODO
                pcl.advancePhase();

                // Update the offset and phase
                float delta = floorf(pcl.phase);
                offset += delta;
                pcl.phase -= delta;

                // Update the last correlation level
                corrLastLvl = corrLvl;
            }

            // Prepare offset for next buffer of samples
            offset -= count;

            // Update delay buffer
            memmove(buffer, &buffer[count], (_interpTapCount - 1) * sizeof(complex_t));

            return outCount;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        void generateInterpTaps() {
            double bw = 0.5 / (double)_interpPhaseCount;
            dsp::tap<float> lp = dsp::taps::windowedSinc<float>(_interpPhaseCount * _interpTapCount, dsp::math::hzToRads(bw, 1.0), dsp::window::nuttall, _interpPhaseCount);
            interpBank = dsp::multirate::buildPolyphaseBank<float>(_interpPhaseCount, lp);
            taps::free(lp);
        }

        // Interpolator
        dsp::multirate::PolyphaseBank<float> interpBank;
        int _interpPhaseCount;
        int _interpTapCount;
        int offset = 0;
        complex_t* buffer = NULL;
        complex_t* bufStart;
        
        // Control loop
        loop::PhaseControlLoop<float, false> pcl;
        double omega;
        double _omegaGain;
        double _muGain;
        double _omegaRelLimit;
        
        // Autocorrelator
        complex_t corr;
        complex_t* corrProducts = NULL;
        float corrAgcRate;
        float corrAgcInvRate;
        float corrLastLvl = 0;
        float corrPeakR = 0;
        float corrPeak = 0;
        float corrPeakL = 0;

        // Symbol
        complex_t* symbol; // TODO: Will use output stream buffer instead
        int symbolSize;
        int sampCount = 0;
        int measuredSymbolPeriod = 0;
    };
};