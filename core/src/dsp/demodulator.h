#pragma once
#include <dsp/block.h>
#include <volk/volk.h>
#include <dsp/filter.h>
#include <dsp/processing.h>
#include <dsp/routing.h>
#include <spdlog/spdlog.h>
#include <dsp/pll.h>
#include <dsp/clock_recovery.h>
#include <dsp/math.h>
#include <dsp/conversion.h>
#include <dsp/audio.h>
#include <dsp/stereo_fm.h>

#define FAST_ATAN2_COEF1    FL_M_PI / 4.0f
#define FAST_ATAN2_COEF2    3.0f * FAST_ATAN2_COEF1

inline float fast_arctan2(float y, float x) {
    float abs_y = fabsf(y);
    float r, angle;
    if (x == 0.0f && y == 0.0f) { return 0.0f; }
    if (x>=0.0f) {
        r = (x - abs_y) / (x + abs_y);
        angle = FAST_ATAN2_COEF1 - FAST_ATAN2_COEF1 * r;
    }
    else {
        r = (x + abs_y) / (abs_y - x);
        angle = FAST_ATAN2_COEF2 - FAST_ATAN2_COEF1 * r;
    }
    if (y < 0.0f) {
        return -angle;
    }
    return angle;
}

namespace dsp {
    class FloatFMDemod : public generic_block<FloatFMDemod> {
    public:
        FloatFMDemod() {}

        FloatFMDemod(stream<complex_t>* in, float sampleRate, float deviation) { init(in, sampleRate, deviation); }

        void init(stream<complex_t>* in, float sampleRate, float deviation) {
            _in = in;
            _sampleRate = sampleRate;
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FloatFMDemod>::registerInput(_in);
            generic_block<FloatFMDemod>::registerOutput(&out);
            generic_block<FloatFMDemod>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<FloatFMDemod>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FloatFMDemod>::ctrlMtx);
            generic_block<FloatFMDemod>::tempStop();
            generic_block<FloatFMDemod>::unregisterInput(_in);
            _in = in;
            generic_block<FloatFMDemod>::registerInput(_in);
            generic_block<FloatFMDemod>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            assert(generic_block<FloatFMDemod>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FloatFMDemod>::ctrlMtx);
            generic_block<FloatFMDemod>::tempStop();
            _sampleRate = sampleRate;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FloatFMDemod>::tempStart();
        }

        float getSampleRate() {
            assert(generic_block<FloatFMDemod>::_block_init);
            return _sampleRate;
        }

        void setDeviation(float deviation) {
            assert(generic_block<FloatFMDemod>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FloatFMDemod>::ctrlMtx);
            generic_block<FloatFMDemod>::tempStop();
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FloatFMDemod>::tempStart();
        }

        float getDeviation() {
            assert(generic_block<FloatFMDemod>::_block_init);
            return _deviation;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // This is somehow faster than volk...
            float diff, currentPhase;
            for (int i = 0; i < count; i++) {
                currentPhase = fast_arctan2(_in->readBuf[i].im, _in->readBuf[i].re);
                diff = currentPhase - phase;
                if (diff > 3.1415926535f)        { diff -= 2 * 3.1415926535f; }
                else if (diff <= -3.1415926535f) { diff += 2 * 3.1415926535f; }
                out.writeBuf[i] = diff / phasorSpeed;
                phase = currentPhase;
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        float phase = 0;
        float phasorSpeed, _sampleRate, _deviation;
        stream<complex_t>* _in;

    };

    class FMDemod : public generic_block<FMDemod> {
    public:
        FMDemod() {}

        FMDemod(stream<complex_t>* in, float sampleRate, float deviation) { init(in, sampleRate, deviation); }

        void init(stream<complex_t>* in, float sampleRate, float deviation) {
            _in = in;
            _sampleRate = sampleRate;
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FMDemod>::registerInput(_in);
            generic_block<FMDemod>::registerOutput(&out);
            generic_block<FMDemod>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<FMDemod>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FMDemod>::ctrlMtx);
            generic_block<FMDemod>::tempStop();
            generic_block<FMDemod>::unregisterInput(_in);
            _in = in;
            generic_block<FMDemod>::registerInput(_in);
            generic_block<FMDemod>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            assert(generic_block<FMDemod>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FMDemod>::ctrlMtx);
            generic_block<FMDemod>::tempStop();
            _sampleRate = sampleRate;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FMDemod>::tempStart();
        }

        float getSampleRate() {
            assert(generic_block<FMDemod>::_block_init);
            return _sampleRate;
        }

        void setDeviation(float deviation) {
            assert(generic_block<FMDemod>::_block_init);
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
        }

        float getDeviation() {
            assert(generic_block<FMDemod>::_block_init);
            return _deviation;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // This is somehow faster than volk...

            float diff, currentPhase;
            for (int i = 0; i < count; i++) {
                currentPhase = fast_arctan2(_in->readBuf[i].im, _in->readBuf[i].re);
                diff = currentPhase - phase;
                if (diff > 3.1415926535f)        { diff -= 2 * 3.1415926535f; }
                else if (diff <= -3.1415926535f) { diff += 2 * 3.1415926535f; }
                out.writeBuf[i].l = diff / phasorSpeed;
                out.writeBuf[i].r = diff / phasorSpeed;
                phase = currentPhase;
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<stereo_t> out;

    private:
        float phase = 0;
        float phasorSpeed, _sampleRate, _deviation;
        stream<complex_t>* _in;

    };

    class AMDemod : public generic_block<AMDemod> {
    public:
        AMDemod() {}

        AMDemod(stream<complex_t>* in) { init(in); }

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<AMDemod>::registerInput(_in);
            generic_block<AMDemod>::registerOutput(&out);
            generic_block<AMDemod>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<AMDemod>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<AMDemod>::ctrlMtx);
            generic_block<AMDemod>::tempStop();
            generic_block<AMDemod>::unregisterInput(_in);
            _in = in;
            generic_block<AMDemod>::registerInput(_in);
            generic_block<AMDemod>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_magnitude_32f(out.writeBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();

            for (int i = 0; i < count; i++) {
                out.writeBuf[i] -= avg;
                avg += out.writeBuf[i] * 10e-4;
            }

            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        stream<complex_t>* _in;
        float avg = 0;

    };

    class SSBDemod : public generic_block<SSBDemod> {
    public:
        SSBDemod() {}

        SSBDemod(stream<complex_t>* in, float sampleRate, float bandWidth, int mode) { init(in, sampleRate, bandWidth, mode); }

        ~SSBDemod() {
            if (!generic_block<SSBDemod>::_block_init) { return; }
            generic_block<SSBDemod>::stop();
            delete[] buffer;
            generic_block<SSBDemod>::_block_init = false;
        }

        enum {
            MODE_USB,
            MODE_LSB,
            MODE_DSB
        };

        void init(stream<complex_t>* in, float sampleRate, float bandWidth, int mode) {
            _in = in;
            _sampleRate = sampleRate;
            _bandWidth = bandWidth;
            _mode = mode;
            phase = lv_cmake(1.0f, 0.0f);
            switch (_mode) {
            case MODE_USB:
                phaseDelta = lv_cmake(std::cos((_bandWidth / _sampleRate) * FL_M_PI), std::sin((_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_LSB:
                phaseDelta = lv_cmake(std::cos(-(_bandWidth / _sampleRate) * FL_M_PI), std::sin(-(_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_DSB:
                phaseDelta = lv_cmake(1.0f, 0.0f);
                break;
            }
            buffer = new lv_32fc_t[STREAM_BUFFER_SIZE];
            generic_block<SSBDemod>::registerInput(_in);
            generic_block<SSBDemod>::registerOutput(&out);
            generic_block<SSBDemod>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<SSBDemod>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<SSBDemod>::ctrlMtx);
            generic_block<SSBDemod>::tempStop();
            generic_block<SSBDemod>::unregisterInput(_in);
            _in = in;
            generic_block<SSBDemod>::registerInput(_in);
            generic_block<SSBDemod>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            assert(generic_block<SSBDemod>::_block_init);
            _sampleRate = sampleRate;
            switch (_mode) {
            case MODE_USB:
                phaseDelta = lv_cmake(std::cos((_bandWidth / _sampleRate) * FL_M_PI), std::sin((_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_LSB:
                phaseDelta = lv_cmake(std::cos(-(_bandWidth / _sampleRate) * FL_M_PI), std::sin(-(_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_DSB:
                phaseDelta = lv_cmake(1.0f, 0.0f);
                break;
            }
        }

        void setBandWidth(float bandWidth) {
            assert(generic_block<SSBDemod>::_block_init);
            _bandWidth = bandWidth;
            switch (_mode) {
            case MODE_USB:
                phaseDelta = lv_cmake(std::cos((_bandWidth / _sampleRate) * FL_M_PI), std::sin((_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_LSB:
                phaseDelta = lv_cmake(std::cos(-(_bandWidth / _sampleRate) * FL_M_PI), std::sin(-(_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_DSB:
                phaseDelta = lv_cmake(1.0f, 0.0f);
                break;
            }
        }

        void setMode(int mode) {
            assert(generic_block<SSBDemod>::_block_init);
            _mode = mode;
            switch (_mode) {
            case MODE_USB:
                phaseDelta = lv_cmake(std::cos((_bandWidth / _sampleRate) * FL_M_PI), std::sin((_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_LSB:
                phaseDelta = lv_cmake(std::cos(-(_bandWidth / _sampleRate) * FL_M_PI), std::sin(-(_bandWidth / _sampleRate) * FL_M_PI));
                break;
            case MODE_DSB:
                phaseDelta = lv_cmake(1.0f, 0.0f);
                break;
            }
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_s32fc_x2_rotator_32fc(buffer, (lv_32fc_t*)_in->readBuf, phaseDelta, &phase, count);
            volk_32fc_deinterleave_real_32f(out.writeBuf, buffer, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        int _mode;
        float _sampleRate, _bandWidth;
        stream<complex_t>* _in;
        lv_32fc_t* buffer;
        lv_32fc_t phase;
        lv_32fc_t phaseDelta;

    };

    class MSKDemod : public generic_hier_block<MSKDemod> {
    public:
        MSKDemod() {}

        MSKDemod(stream<complex_t>* input, float sampleRate, float deviation, float baudRate, float omegaGain = (0.01*0.01) / 4, float muGain = 0.01f, float omegaRelLimit = 0.005f) {
            init(input, sampleRate, deviation, baudRate, omegaGain, muGain, omegaRelLimit);
        }

        void init(stream<complex_t>* input, float sampleRate, float deviation, float baudRate, float omegaGain = (0.01*0.01) / 4, float muGain = 0.01f, float omegaRelLimit = 0.005f) {
            _sampleRate = sampleRate;
            _deviation = deviation;
            _baudRate = baudRate;
            _omegaGain = omegaGain;
            _muGain = muGain;
            _omegaRelLimit = omegaRelLimit;
            
            demod.init(input, _sampleRate, _deviation);
            recov.init(&demod.out, _sampleRate / _baudRate, _omegaGain, _muGain, _omegaRelLimit);
            out = &recov.out;

            generic_hier_block<MSKDemod>::registerBlock(&demod);
            generic_hier_block<MSKDemod>::registerBlock(&recov);
            generic_hier_block<MSKDemod>::_block_init = true;
        }

        void setInput(stream<complex_t>* input) {
            assert((generic_hier_block<MSKDemod>::_block_init));
            demod.setInput(input);
        }

        void setSampleRate(float sampleRate) {
            assert(generic_hier_block<MSKDemod>::_block_init);
            generic_hier_block<MSKDemod>::tempStop();
            _sampleRate = sampleRate;
            demod.setSampleRate(_sampleRate);
            recov.setOmega(_sampleRate / _baudRate, _omegaRelLimit);
            generic_hier_block<MSKDemod>::tempStart();
        }

        void setDeviation(float deviation) {
            assert(generic_hier_block<MSKDemod>::_block_init);
            _deviation = deviation;
            demod.setDeviation(deviation);
        }

        void setBaudRate(float baudRate, float omegaRelLimit) {
            assert(generic_hier_block<MSKDemod>::_block_init);
            _baudRate = baudRate;
            _omegaRelLimit = omegaRelLimit;
            recov.setOmega(_sampleRate / _baudRate, _omegaRelLimit);
        }

        void setMMGains(float omegaGain, float myGain) {
            assert(generic_hier_block<MSKDemod>::_block_init);
            _omegaGain = omegaGain;
            _muGain = myGain;
            recov.setGains(_omegaGain, _muGain);
        }

        void setOmegaRelLimit(float omegaRelLimit) {
            assert(generic_hier_block<MSKDemod>::_block_init);
            _omegaRelLimit = omegaRelLimit;
            recov.setOmegaRelLimit(_omegaRelLimit);
        }

        stream<float>* out = NULL;

    private:
        FloatFMDemod demod;
        MMClockRecovery<float> recov;

        float _sampleRate;
        float _deviation;
        float _baudRate;
        float _omegaGain;
        float _muGain;
        float _omegaRelLimit;
    };

    template<int ORDER, bool OFFSET>
    class PSKDemod : public generic_hier_block<PSKDemod<ORDER, OFFSET>> {
    public:
        PSKDemod() {}

        PSKDemod(stream<complex_t>* input, float sampleRate, float baudRate, int RRCTapCount = 31, float RRCAlpha = 0.32f, float agcRate = 10e-4, float costasLoopBw = 0.004f, float omegaGain = (0.01*0.01) / 4, float muGain = 0.01f, float omegaRelLimit = 0.005f) {
            init(input, sampleRate, baudRate, RRCTapCount, RRCAlpha, agcRate, costasLoopBw, omegaGain, muGain, omegaRelLimit);
        }

        void init(stream<complex_t>* input, float sampleRate, float baudRate, int RRCTapCount = 31, float RRCAlpha = 0.32f, float agcRate = 10e-4, float costasLoopBw = 0.004f, float omegaGain = (0.01*0.01) / 4, float muGain = 0.01f, float omegaRelLimit = 0.005f) {
            _RRCTapCount = RRCTapCount;
            _RRCAlpha = RRCAlpha;
            _sampleRate = sampleRate;
            _agcRate = agcRate;
            _costasLoopBw = costasLoopBw;
            _baudRate = baudRate;
            _omegaGain = omegaGain;
            _muGain = muGain;
            _omegaRelLimit = omegaRelLimit;
            
            agc.init(input, 1.0f, 65535, _agcRate);
            taps.init(_RRCTapCount, _sampleRate, _baudRate, _RRCAlpha);
            rrc.init(&agc.out, &taps);
            demod.init(&rrc.out, _costasLoopBw);

            generic_hier_block<PSKDemod<ORDER, OFFSET>>::registerBlock(&agc);
            generic_hier_block<PSKDemod<ORDER, OFFSET>>::registerBlock(&rrc);
            generic_hier_block<PSKDemod<ORDER, OFFSET>>::registerBlock(&demod);

            if constexpr (OFFSET) {
                delay.init(&demod.out);
                recov.init(&delay.out, _sampleRate / _baudRate, _omegaGain, _muGain, _omegaRelLimit);
                generic_hier_block<PSKDemod<ORDER, OFFSET>>::registerBlock(&delay);
            }   
            else {
                recov.init(&demod.out, _sampleRate / _baudRate, _omegaGain, _muGain, _omegaRelLimit);
            }

            generic_hier_block<PSKDemod<ORDER, OFFSET>>::registerBlock(&recov);

            out = &recov.out;

            generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init = true;
        }

        void setInput(stream<complex_t>* input) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            agc.setInput(input);
        }

        void setSampleRate(float sampleRate) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            _sampleRate = sampleRate;
            rrc.tempStop();
            recov.tempStop();
            taps.setSampleRate(_sampleRate);
            rrc.updateWindow(&taps);
            recov.setOmega(_sampleRate / _baudRate, _omegaRelLimit);
            rrc.tempStart();
            recov.tempStart();
        }

        void setBaudRate(float baudRate) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            _baudRate = baudRate;
            rrc.tempStop();
            recov.tempStop();
            taps.setBaudRate(_baudRate);
            rrc.updateWindow(&taps);
            recov.setOmega(_sampleRate / _baudRate, _omegaRelLimit);
            rrc.tempStart();
            recov.tempStart();
        }

        void setRRCParams(int RRCTapCount, float RRCAlpha) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            _RRCTapCount = RRCTapCount;
            _RRCAlpha = RRCAlpha;
            taps.setTapCount(_RRCTapCount);
            taps.setAlpha(RRCAlpha);
            rrc.updateWindow(&taps);
        }

        void setAgcRate(float agcRate) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            _agcRate = agcRate;
            agc.setRate(_agcRate);
        }

        void setCostasLoopBw(float costasLoopBw) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            _costasLoopBw = costasLoopBw;
            demod.setLoopBandwidth(_costasLoopBw);
        }

        void setMMGains(float omegaGain, float myGain) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            _omegaGain = omegaGain;
            _muGain = myGain;
            recov.setGains(_omegaGain, _muGain);
        }

        void setOmegaRelLimit(float omegaRelLimit) {
            assert((generic_hier_block<PSKDemod<ORDER, OFFSET>>::_block_init));
            _omegaRelLimit = omegaRelLimit;
            recov.setOmegaRelLimit(_omegaRelLimit);
        }

        stream<complex_t>* out = NULL;

    private:
        dsp::ComplexAGC agc;
        dsp::RRCTaps taps;
        dsp::FIR<dsp::complex_t> rrc;
        CostasLoop<ORDER> demod;
        DelayImag delay;
        MMClockRecovery<dsp::complex_t> recov;

        int _RRCTapCount;
        float _RRCAlpha;
        float _sampleRate;
        float _agcRate;
        float _baudRate;
        float _costasLoopBw;
        float _omegaGain;
        float _muGain;
        float _omegaRelLimit;
    };

    class PMDemod : public generic_hier_block<PMDemod> {
    public:
        PMDemod() {}

        PMDemod(stream<complex_t>* input, float sampleRate, float baudRate, float agcRate = 0.02e-3f, float pllLoopBandwidth = (0.06f*0.06f) / 4.0f, int rrcTapCount = 31, float rrcAlpha = 0.6f, float omegaGain = (0.01*0.01) / 4, float muGain = 0.01f, float omegaRelLimit = 0.005f) {
            init(input, sampleRate, baudRate, agcRate, pllLoopBandwidth, rrcTapCount, rrcAlpha, omegaGain, muGain, omegaRelLimit);
        }

        void init(stream<complex_t>* input, float sampleRate, float baudRate, float agcRate = 0.02e-3f, float pllLoopBandwidth = (0.06f*0.06f) / 4.0f, int rrcTapCount = 31, float rrcAlpha = 0.6f, float omegaGain = (0.01*0.01) / 4, float muGain = 0.01f, float omegaRelLimit = 0.005f) {
            _sampleRate = sampleRate;
            _baudRate = baudRate;
            _agcRate = agcRate;
            _pllLoopBandwidth = pllLoopBandwidth;
            _rrcTapCount = rrcTapCount;
            _rrcAlpha = rrcAlpha;
            _omegaGain = omegaGain;
            _muGain = muGain;
            _omegaRelLimit = omegaRelLimit;
            
            agc.init(input, 1.0f, 65535, _agcRate);
            pll.init(&agc.out, _pllLoopBandwidth);
            rrcwin.init(_rrcTapCount, _sampleRate, _baudRate, _rrcAlpha);
            rrc.init(&pll.out, &rrcwin);
            recov.init(&rrc.out, _sampleRate / _baudRate, _omegaGain, _muGain, _omegaRelLimit);

            out = &recov.out;

            generic_hier_block<PMDemod>::registerBlock(&agc);
            generic_hier_block<PMDemod>::registerBlock(&pll);
            generic_hier_block<PMDemod>::registerBlock(&rrc);
            generic_hier_block<PMDemod>::registerBlock(&recov);
            generic_hier_block<PMDemod>::_block_init = true;
        }

        void setInput(stream<complex_t>* input) {
            assert(generic_hier_block<PMDemod>::_block_init);
            agc.setInput(input);
        }

        void setAgcRate(float agcRate) {
            assert(generic_hier_block<PMDemod>::_block_init);
            _agcRate = agcRate;
            agc.setRate(_agcRate);
        }

        void setPllLoopBandwidth(float pllLoopBandwidth) {
            assert(generic_hier_block<PMDemod>::_block_init);
            _pllLoopBandwidth = pllLoopBandwidth;
            pll.setLoopBandwidth(_pllLoopBandwidth);
        }

        void setRRCParams(int rrcTapCount, float rrcAlpha) {
            assert(generic_hier_block<PMDemod>::_block_init);
            _rrcTapCount = rrcTapCount;
            _rrcAlpha = rrcAlpha;
            rrcwin.setTapCount(_rrcTapCount);
            rrcwin.setAlpha(_rrcAlpha);
            rrc.updateWindow(&rrcwin);
        }

        void setMMGains(float omegaGain, float muGain) {
            assert(generic_hier_block<PMDemod>::_block_init);
            _omegaGain = omegaGain;
            _muGain = muGain;
            recov.setGains(_omegaGain, _muGain);
        }

        void setOmegaRelLimit(float omegaRelLimit) {
            assert(generic_hier_block<PMDemod>::_block_init);
            _omegaRelLimit = omegaRelLimit;
            recov.setOmegaRelLimit(_omegaRelLimit);
        }

        stream<float>* out = NULL;

    private:
        dsp::ComplexAGC agc;
        dsp::CarrierTrackingPLL<float> pll;
        dsp::RRCTaps rrcwin;
        dsp::FIR<float> rrc;
        dsp::MMClockRecovery<float> recov;

        float _sampleRate;
        float _baudRate;
        float _agcRate;
        float _pllLoopBandwidth;
        int _rrcTapCount;
        float _rrcAlpha;
        float _omegaGain;
        float _muGain;
        float _omegaRelLimit;
    };

    class StereoFMDemod : public generic_hier_block<StereoFMDemod> {
    public:
        StereoFMDemod() {}

        StereoFMDemod(stream<complex_t>* input, float sampleRate, float deviation) {
            init(input, sampleRate, deviation);
        }

        void init(stream<complex_t>* input, float sampleRate, float deviation) {
            _sampleRate = sampleRate;

            PilotFirWin.init(18750, 19250, 3000, _sampleRate);

            demod.init(input, _sampleRate, deviation);

            r2c.init(&demod.out);

            pilotFilter.init(&r2c.out, &PilotFirWin);

            demux.init(&pilotFilter.dataOut, &pilotFilter.pilotOut, 0.1f);

            recon.init(&demux.AplusBOut, &demux.AminusBOut);

            out = &recon.out;
            
            generic_hier_block<StereoFMDemod>::registerBlock(&demod);
            generic_hier_block<StereoFMDemod>::registerBlock(&r2c);
            generic_hier_block<StereoFMDemod>::registerBlock(&pilotFilter);
            generic_hier_block<StereoFMDemod>::registerBlock(&demux);
            generic_hier_block<StereoFMDemod>::registerBlock(&recon);
            generic_hier_block<StereoFMDemod>::_block_init = true;
        }

        void setInput(stream<float>* input) {
            assert(generic_hier_block<StereoFMDemod>::_block_init);
            r2c.setInput(input);
        }

        void setDeviation(float deviation) {
            demod.setDeviation(deviation);
        }

        stream<stereo_t>* out = NULL;

    private:
        filter_window::BandPassBlackmanWindow PilotFirWin;

        FloatFMDemod demod;

        RealToComplex r2c;

        FMStereoDemuxPilotFilter pilotFilter;

        FMStereoDemux demux;

        FMStereoReconstruct recon;

        float _sampleRate;
    };
}