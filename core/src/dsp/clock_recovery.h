#pragma once
#include <dsp/block.h>
#include <dsp/utils/macros.h>
#include <dsp/interpolation_taps.h>

namespace dsp {
    class EdgeTrigClockRecovery : public generic_block<EdgeTrigClockRecovery> {
    public:
        EdgeTrigClockRecovery() {}
        
        EdgeTrigClockRecovery(stream<float>* in, int omega) { init(in, omega); }

        void init(stream<float>* in, int omega) {
            _in = in;
            samplesPerSymbol = omega;
            generic_block<EdgeTrigClockRecovery>::registerInput(_in);
            generic_block<EdgeTrigClockRecovery>::registerOutput(&out);
            generic_block<EdgeTrigClockRecovery>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<EdgeTrigClockRecovery>::_block_init);
            generic_block<EdgeTrigClockRecovery>::tempStop();
            generic_block<EdgeTrigClockRecovery>::unregisterInput(_in);
            _in = in;
            generic_block<EdgeTrigClockRecovery>::registerInput(_in);
            generic_block<EdgeTrigClockRecovery>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            int outCount = 0;

            for (int i = 0; i < count; i++) {
                if (DSP_SIGN(lastVal) != DSP_SIGN(_in->readBuf[i])) {
                    counter = samplesPerSymbol / 2;
                    lastVal = _in->readBuf[i];
                    continue;
                }

                if (counter >= samplesPerSymbol) {
                    counter = 0;
                    out.writeBuf[outCount] = _in->readBuf[i];
                    outCount++;
                }
                else {
                    counter++;
                }

                lastVal = _in->readBuf[i];
            }
            
            _in->flush();
            if (outCount > 0 && !out.swap(outCount)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        int count;
        int samplesPerSymbol = 1;
        int counter = 0;
        float lastVal = 0;
        stream<float>* _in;

    };

    template<class T>
    class MMClockRecovery : public generic_block<MMClockRecovery<T>> {
    public:
        MMClockRecovery() {}

        MMClockRecovery(stream<T>* in, float omega, float gainOmega, float muGain, float omegaRelLimit) {
            init(in, omega, gainOmega, muGain, omegaRelLimit);
        }

        void init(stream<T>* in, float omega, float gainOmega, float muGain, float omegaRelLimit) {
            _in = in;
            _omega = omega;
            _muGain = muGain;
            _gainOmega = gainOmega;
            _omegaRelLimit = omegaRelLimit;

            omegaMin = _omega - (_omega * _omegaRelLimit);
            omegaMax = _omega + (_omega * _omegaRelLimit);
            _dynOmega = _omega;

            memset(delay, 0, 1024 * sizeof(T));

            generic_block<MMClockRecovery<T>>::registerInput(_in);
            generic_block<MMClockRecovery<T>>::registerOutput(&out);
            generic_block<MMClockRecovery<T>>::_block_init = true;
        }

        void setOmega(float omega, float omegaRelLimit) {
            assert(generic_block<MMClockRecovery<T>>::_block_init);
            generic_block<MMClockRecovery<T>>::tempStop();
            omegaMin = _omega - (_omega * _omegaRelLimit);
            omegaMax = _omega + (_omega * _omegaRelLimit);
            _omega = omega;
            _dynOmega = _omega;
            generic_block<MMClockRecovery<T>>::tempStart();
        }

        void setGains(float omegaGain, float muGain) {
            assert(generic_block<MMClockRecovery<T>>::_block_init);
            generic_block<MMClockRecovery<T>>::tempStop();
            _gainOmega = omegaGain;
            _muGain = muGain;
            generic_block<MMClockRecovery<T>>::tempStart();
        }

        void setOmegaRelLimit(float omegaRelLimit) {
            assert(generic_block<MMClockRecovery<T>>::_block_init);
            generic_block<MMClockRecovery<T>>::tempStop();
            _omegaRelLimit = omegaRelLimit;
            omegaMin = _omega - (_omega * _omegaRelLimit);
            omegaMax = _omega + (_omega * _omegaRelLimit);
            generic_block<MMClockRecovery<T>>::tempStart();
        }

        void setInput(stream<T>* in) {
            assert(generic_block<MMClockRecovery<T>>::_block_init);
            generic_block<MMClockRecovery<T>>::tempStop();
            generic_block<MMClockRecovery<T>>::unregisterInput(_in);
            _in = in;
            generic_block<MMClockRecovery<T>>::registerInput(_in);
            generic_block<MMClockRecovery<T>>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            int outCount = 0;
            float outVal;
            float phaseError;
            float roundedStep;
            int maxOut = 2.0f * _omega * (float)count;

            // Copy the first 7 values to the delay buffer for fast computing
            memcpy(&delay[7], _in->readBuf, 7 * sizeof(T));

            int i = nextOffset;
            for (; i < count && outCount < maxOut;) {
                
                if constexpr (std::is_same_v<T, float>) {
                    // Calculate output value
                    // If we still need to use the old values, calculate using delay buf
                    // Otherwise, use normal buffer
                    if (i < 7) {
                        volk_32f_x2_dot_prod_32f(&outVal, &delay[i], INTERP_TAPS[(int)roundf(_mu * 128.0f)], 8);
                    }
                    else {
                        volk_32f_x2_dot_prod_32f(&outVal, &_in->readBuf[i - 7], INTERP_TAPS[(int)roundf(_mu * 128.0f)], 8);
                    }
                    out.writeBuf[outCount++] = outVal;

                    // Cursed phase detect approximation (don't ask me how this approximation works)
                    phaseError = (DSP_STEP(lastOutput)*outVal) - (lastOutput*DSP_STEP(outVal));
                    lastOutput = outVal;
                }
                if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                    // Propagate delay
                    _p_2T = _p_1T;
                    _p_1T = _p_0T;

                    _c_2T = _c_1T;
                    _c_1T = _c_0T;

                    // Perfrom interpolation the same way as for float values
                    if (i < 7) {
                        volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&_p_0T, (lv_32fc_t*)&delay[i], INTERP_TAPS[(int)roundf(_mu * 128.0f)], 8);
                    }
                    else {
                        volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&_p_0T, (lv_32fc_t*)&_in->readBuf[i - 7], INTERP_TAPS[(int)roundf(_mu * 128.0f)], 8);
                    }
                    out.writeBuf[outCount++] = _p_0T;

                    // Slice output value
                    _c_0T = DSP_STEP_CPLX(_p_0T);

                    // Cursed math to calculate the phase error
                    phaseError = (((_p_0T - _p_2T) * _c_1T.conj()) - ((_c_0T - _c_2T) * _p_1T.conj())).re;
                }

                // Clamp phase error
                if (phaseError > 1.0f) { phaseError = 1.0f; }
                if (phaseError < -1.0f) { phaseError = -1.0f; }

                // Adjust the symbol rate using the phase error approximation and clamp
                // TODO: Branchless clamp
                _dynOmega = _dynOmega + (_gainOmega * phaseError);
                if (_dynOmega > omegaMax) { _dynOmega = omegaMax; }
                else if (_dynOmega < omegaMin) { _dynOmega = omegaMin; }

                // Adjust the symbol phase according to the phase error approximation
                // It will now contain the phase delta needed to jump to the next symbol
                // Rounded step will contain the rounded number of symbols
                _mu = _mu + _dynOmega + (_muGain * phaseError);
                roundedStep = floor(_mu);

                // Step to where the next symbol should be, and check for bogus input
                i += (int)roundedStep;
                if (i < 0) { i = 0; }

                // Now that we've stepped to the next symbol, keep only the offset inside the symbol
                _mu -= roundedStep;
            }

            nextOffset = i - count;

            // Save the last 7 values for the next round
            memcpy(delay, &_in->readBuf[count - 7], 7 * sizeof(T));
            
            _in->flush();
            if (outCount > 0 && !out.swap(outCount)) { return -1; }
            return count;
        }

        stream<T> out;

    private:
        int count;

        // Delay buffer
        T delay[1024];
        int nextOffset = 0;

        // Configuration
        float _omega = 1.0f;
        float _muGain = 1.0f;
        float _gainOmega = 0.001f;
        float _omegaRelLimit = 0.005;

        // Precalculated values
        float omegaMin = _omega + (_omega * _omegaRelLimit);
        float omegaMax = _omega + (_omega * _omegaRelLimit);

        // Runtime adjusted
        float _dynOmega = _omega;
        float _mu = 0.5f;
        float lastOutput = 0.0f;

        // Cursed complex stuff
        complex_t _p_0T = {0,0}, _p_1T = {0,0}, _p_2T = {0,0};
        complex_t _c_0T = {0,0}, _c_1T = {0,0}, _c_2T = {0,0};

        stream<T>* _in;

    };
}