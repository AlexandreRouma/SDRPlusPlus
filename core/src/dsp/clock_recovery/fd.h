#pragma once
#include "../processor.h"
#include "../loop/phase_control_loop.h"
#include "../taps/windowed_sinc.h"
#include "../multirate/polyphase_bank.h"
#include "../math/step.h"

namespace dsp::clock_recovery {
    class FD : public Processor<float, float> {
        using base_type = Processor<float, float> ;
    public:
        FD() {}

        FD(stream<float>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) { init(in, omega, omegaGain, muGain, omegaRelLimit, interpPhaseCount, interpTapCount); }

        ~FD() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            dsp::multirate::freePolyphaseBank(interpBank);
            buffer::free(buffer);
        }

        void init(stream<float>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) {
            _omega = omega;
            _omegaGain = omegaGain;
            _muGain = muGain;
            _omegaRelLimit = omegaRelLimit;
            _interpPhaseCount = interpPhaseCount;
            _interpTapCount = interpTapCount;

            pcl.init(_muGain, _omegaGain, 0.0, 0.0, 1.0, _omega, _omega * (1.0 - omegaRelLimit), _omega * (1.0 + omegaRelLimit));
            generateInterpTaps();
            buffer = buffer::alloc<float>(STREAM_BUFFER_SIZE + _interpTapCount);
            bufStart = &buffer[_interpTapCount - 1];
        
            base_type::init(in);
        }

        void setOmega(double omega) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _omega = omega;
            offset = 0;
            pcl.phase = 0.0f;
            pcl.freq = _omega;
            pcl.setFreqLimits(_omega * (1.0 - _omegaRelLimit), _omega * (1.0 + _omegaRelLimit));
            base_type::tempStart();
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
            pcl.setFreqLimits(_omega * (1.0 - _omegaRelLimit), _omega * (1.0 + _omegaRelLimit));
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
            buffer = buffer::alloc<float>(STREAM_BUFFER_SIZE + _interpTapCount);
            bufStart = &buffer[_interpTapCount - 1];
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            offset = 0;
            pcl.phase = 0.0f;
            pcl.freq = _omega;
            base_type::tempStart();
        }

        inline int process(int count, const float* in, float* out) {
            // Copy data to work buffer
            memcpy(bufStart, in, count * sizeof(float));

            // Process all samples
            int outCount = 0;
            while (offset < count) {
                float error;
                float outVal;
                float dfdt;

                // Calculate new output value
                int phase = std::clamp<int>(floorf(pcl.phase * (float)_interpPhaseCount), 0, _interpPhaseCount - 1);
                volk_32f_x2_dot_prod_32f(&outVal, &buffer[offset], interpBank.phases[phase], _interpTapCount);
                out[outCount++] = outVal;

                // Calculate derivative of the signal
                if (phase == 0) {
                    float fT1;
                    volk_32f_x2_dot_prod_32f(&fT1, &buffer[offset], interpBank.phases[phase+1], _interpTapCount);
                    dfdt = fT1 - outVal;
                }
                else if (phase == _interpPhaseCount - 1) {
                    float fT_1;
                    volk_32f_x2_dot_prod_32f(&fT_1, &buffer[offset], interpBank.phases[phase-1], _interpTapCount);
                    dfdt = outVal - fT_1;
                }
                else {
                    float fT_1;
                    float fT1;
                    volk_32f_x2_dot_prod_32f(&fT_1, &buffer[offset], interpBank.phases[phase-1], _interpTapCount);
                    volk_32f_x2_dot_prod_32f(&fT1, &buffer[offset], interpBank.phases[phase+1], _interpTapCount);
                    dfdt = (fT1 - fT_1) * 0.5f;
                }
                
                // Calculate error
                error = dfdt * math::step(outVal);

                // Clamp symbol phase error
                if (error > 1.0f) { error = 1.0f; }
                if (error < -1.0f) { error = -1.0f; }

                // Advance symbol offset and phase
                pcl.advance(error);
                float delta = floorf(pcl.phase);
                offset += delta;
                pcl.phase -= delta;
            }
            offset -= count;

            // Update delay buffer
            memmove(buffer, &buffer[count], (_interpTapCount - 1) * sizeof(float));

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

        loop::PhaseControlLoop<float, false> pcl;

    protected:
        void generateInterpTaps() {
            double bw = 0.5 / (double)_interpPhaseCount;
            dsp::tap<float> lp = dsp::taps::windowedSinc<float>(_interpPhaseCount * _interpTapCount, dsp::math::freqToOmega(bw, 1.0), dsp::window::nuttall, _interpPhaseCount);
            interpBank = dsp::multirate::buildPolyphaseBank<float>(_interpPhaseCount, lp);
            taps::free(lp);
        }

        dsp::multirate::PolyphaseBank<float> interpBank;

        double _omega;
        double _omegaGain;
        double _muGain;
        double _omegaRelLimit;
        int _interpPhaseCount;
        int _interpTapCount;

        int offset = 0;
        float* buffer;
        float* bufStart;
    };
}