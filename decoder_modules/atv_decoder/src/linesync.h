#pragma once
#include <dsp/processor.h>
#include <dsp/loop/phase_control_loop.h>
#include <dsp/taps/windowed_sinc.h>
#include <dsp/multirate/polyphase_bank.h>
#include <dsp/math/step.h>

class LineSync : public dsp::Processor<float, float> {
    using base_type = dsp::Processor<float, float>;
public:
    LineSync() {}

    LineSync(dsp::stream<float>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) { init(in, omega, omegaGain, muGain, omegaRelLimit, interpPhaseCount, interpTapCount); }

    ~LineSync() {
        if (!base_type::_block_init) { return; }
        base_type::stop();
        dsp::multirate::freePolyphaseBank(interpBank);
        dsp::buffer::free(buffer);
    }

    void init(dsp::stream<float>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) {
        _omega = omega;
        _omegaGain = omegaGain;
        _muGain = muGain;
        _omegaRelLimit = omegaRelLimit;
        _interpPhaseCount = interpPhaseCount;
        _interpTapCount = interpTapCount;

        pcl.init(_muGain, _omegaGain, 0.0, 0.0, 1.0, _omega, _omega * (1.0 - omegaRelLimit), _omega * (1.0 + omegaRelLimit));
        generateInterpTaps();
        buffer = dsp::buffer::alloc<float>(STREAM_BUFFER_SIZE + _interpTapCount);
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
        pcl.setFreqLimits(_omega * (1.0 - _omegaRelLimit), _omega * (1.0 + _omegaRelLimit));
    }

    void setSyncLevel(float level) {
        assert(base_type::_block_init);
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        syncLevel = level;
    }

    void setInterpParams(int interpPhaseCount, int interpTapCount) {
        assert(base_type::_block_init);
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        base_type::tempStop();
        _interpPhaseCount = interpPhaseCount;
        _interpTapCount = interpTapCount;
        dsp::multirate::freePolyphaseBank(interpBank);
        dsp::buffer::free(buffer);
        generateInterpTaps();
        buffer = dsp::buffer::alloc<float>(STREAM_BUFFER_SIZE + _interpTapCount);
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

    int run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        // Copy data to work buffer
        memcpy(bufStart, base_type::_in->readBuf, count * sizeof(float));

        if (test2) {
            test2 = false;
            offset += 5;
        }

        // Process all samples
        while (offset < count) {
            // Calculate new output value
            int phase = std::clamp<int>(floorf(pcl.phase * (float)_interpPhaseCount), 0, _interpPhaseCount - 1);
            float outVal;
            volk_32f_x2_dot_prod_32f(&outVal, &buffer[offset], interpBank.phases[phase], _interpTapCount);
            base_type::out.writeBuf[outCount++] = outVal;

            // If the end of the line is reached, process it and determin error
            float error = 0;
            if (outCount >= 720) {
                // Compute averages.
                float left = 0.0f, right = 0.0f;
                for (int i = (720-17); i < 720; i++) {
                    left += base_type::out.writeBuf[i];
                }
                for (int i = 0; i < 27; i++) {
                    left += base_type::out.writeBuf[i];
                }
                for (int i = 27; i < (54+17); i++) {
                    right += base_type::out.writeBuf[i];
                }
                left *= (1.0f/44.0f);
                right *= (1.0f/44.0f);

                // If the sync is present, compute error
                if ((left < syncLevel && right < syncLevel) && !forceLock) {
                    error = (left + syncBias - right);
                    locked = true;
                }
                else {
                    locked = false;
                }

                if (++counter >= 100) {
                    counter = 0;
                    //flog::warn("Left: {}, Right: {}, Error: {}, Freq: {}, Phase: {}", left, right, error, pcl.freq, pcl.phase);
                }

                // Output line
                if (!base_type::out.swap(outCount)) { break; }
                outCount = 0;
            }

            // Advance symbol offset and phase
            pcl.advance(error);
            float delta = floorf(pcl.phase);
            offset += delta;
            pcl.phase -= delta;
        }
        offset -= count;

        // Update delay buffer
        memmove(buffer, &buffer[count], (_interpTapCount - 1) * sizeof(float));

        // Swap if some data was generated
        base_type::_in->flush();
        return outCount;
    }

    bool locked = false;
    bool test2 = false;

    float syncBias = 0.0f;
    bool forceLock = false;

    int counter = 0;

protected:
    void generateInterpTaps() {
        double bw = 0.5 / (double)_interpPhaseCount;
        dsp::tap<float> lp = dsp::taps::windowedSinc<float>(_interpPhaseCount * _interpTapCount, dsp::math::hzToRads(bw, 1.0), dsp::window::nuttall, _interpPhaseCount);
        interpBank = dsp::multirate::buildPolyphaseBank<float>(_interpPhaseCount, lp);
        dsp::taps::free(lp);
    }

    dsp::multirate::PolyphaseBank<float> interpBank;
    dsp::loop::PhaseControlLoop<double, false> pcl;

    double _omega;
    double _omegaGain;
    double _muGain;
    double _omegaRelLimit;
    int _interpPhaseCount;
    int _interpTapCount;

    int offset = 0;
    int outCount = 0;
    float* buffer;
    float* bufStart;
    
    float syncLevel = -0.03f;
};