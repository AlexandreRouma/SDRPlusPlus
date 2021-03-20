#pragma once
#include <dsp/block.h>
#include <volk/volk.h>
#include <dsp/filter.h>
#include <dsp/processing.h>
#include <dsp/routing.h>
#include <spdlog/spdlog.h>

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

        ~FloatFMDemod() { generic_block<FloatFMDemod>::stop(); }

        void init(stream<complex_t>* in, float sampleRate, float deviation) {
            _in = in;
            _sampleRate = sampleRate;
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FloatFMDemod>::registerInput(_in);
            generic_block<FloatFMDemod>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<FloatFMDemod>::ctrlMtx);
            generic_block<FloatFMDemod>::tempStop();
            generic_block<FloatFMDemod>::unregisterInput(_in);
            _in = in;
            generic_block<FloatFMDemod>::registerInput(_in);
            generic_block<FloatFMDemod>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            std::lock_guard<std::mutex> lck(generic_block<FloatFMDemod>::ctrlMtx);
            generic_block<FloatFMDemod>::tempStop();
            _sampleRate = sampleRate;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FloatFMDemod>::tempStart();
        }

        float getSampleRate() {
            return _sampleRate;
        }

        void setDeviation(float deviation) {
            std::lock_guard<std::mutex> lck(generic_block<FloatFMDemod>::ctrlMtx);
            generic_block<FloatFMDemod>::tempStop();
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FloatFMDemod>::tempStart();
        }

        float getDeviation() {
            return _deviation;
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            // This is somehow faster than volk...

            float diff, currentPhase;
            for (int i = 0; i < count; i++) {
                currentPhase = fast_arctan2(_in->readBuf[i].i, _in->readBuf[i].q);
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
        int count;
        float phase, phasorSpeed, _sampleRate, _deviation;
        stream<complex_t>* _in;

    };

    class FMDemod : public generic_block<FMDemod> {
    public:
        FMDemod() {}

        FMDemod(stream<complex_t>* in, float sampleRate, float deviation) { init(in, sampleRate, deviation); }

        ~FMDemod() { generic_block<FMDemod>::stop(); }

        void init(stream<complex_t>* in, float sampleRate, float deviation) {
            _in = in;
            _sampleRate = sampleRate;
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FMDemod>::registerInput(_in);
            generic_block<FMDemod>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<FMDemod>::ctrlMtx);
            generic_block<FMDemod>::tempStop();
            generic_block<FMDemod>::unregisterInput(_in);
            _in = in;
            generic_block<FMDemod>::registerInput(_in);
            generic_block<FMDemod>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            std::lock_guard<std::mutex> lck(generic_block<FMDemod>::ctrlMtx);
            generic_block<FMDemod>::tempStop();
            _sampleRate = sampleRate;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FMDemod>::tempStart();
        }

        float getSampleRate() {
            return _sampleRate;
        }

        void setDeviation(float deviation) {
            std::lock_guard<std::mutex> lck(generic_block<FMDemod>::ctrlMtx);
            generic_block<FMDemod>::tempStop();
            _deviation = deviation;
            phasorSpeed = (2 * FL_M_PI) / (_sampleRate / _deviation);
            generic_block<FMDemod>::tempStart();
        }

        float getDeviation() {
            return _deviation;
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            // This is somehow faster than volk...

            float diff, currentPhase;
            for (int i = 0; i < count; i++) {
                currentPhase = fast_arctan2(_in->readBuf[i].i, _in->readBuf[i].q);
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
        int count;
        float phase, phasorSpeed, _sampleRate, _deviation;
        stream<complex_t>* _in;

    };

    class StereoFMDemod : public generic_block<StereoFMDemod> {
    public:
        StereoFMDemod() {}

        StereoFMDemod(stream<complex_t>* in, float sampleRate, float deviation) { init(in, sampleRate, deviation); }

        ~StereoFMDemod() {
            stop();
            delete[] doubledPilot;
            delete[] a_minus_b;
            delete[] a_out;
            delete[] b_out;
        }

        void init(stream<complex_t>* in, float sampleRate, float deviation) {
            _sampleRate = sampleRate;

            doubledPilot = new float[STREAM_BUFFER_SIZE];
            a_minus_b = new float[STREAM_BUFFER_SIZE];
            a_out = new float[STREAM_BUFFER_SIZE];
            b_out = new float[STREAM_BUFFER_SIZE];

            fmDemod.init(in, sampleRate, deviation);
            split.init(&fmDemod.out);
            split.bindStream(&filterInput);
            split.bindStream(&decodeInput);

            // Filter init
            win.init(1000, 1000, 19000, sampleRate);
            filter.init(&filterInput, &win);
            agc.init(&filter.out, 20.0f, sampleRate);

            generic_block<StereoFMDemod>::registerInput(&decodeInput);
            generic_block<StereoFMDemod>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<StereoFMDemod>::ctrlMtx);
            generic_block<StereoFMDemod>::tempStop();
            fmDemod.setInput(in);
            generic_block<StereoFMDemod>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            std::lock_guard<std::mutex> lck(generic_block<StereoFMDemod>::ctrlMtx);
            generic_block<StereoFMDemod>::tempStop();
            _sampleRate = sampleRate;
            fmDemod.setSampleRate(sampleRate);
            win.setSampleRate(_sampleRate);
            filter.updateWindow(&win);
            generic_block<StereoFMDemod>::tempStart();
        }

        float getSampleRate() {
            return _sampleRate;
        }

        void setDeviation(float deviation) {
            std::lock_guard<std::mutex> lck(generic_block<StereoFMDemod>::ctrlMtx);
            generic_block<StereoFMDemod>::tempStop();
            fmDemod.setDeviation(deviation);
            generic_block<StereoFMDemod>::tempStart();
        }

        float getDeviation() {
            return fmDemod.getDeviation();
        }

        int run() {
            count = decodeInput.read();
            if (count < 0) { return -1; }
            countFilter = agc.out.read();
            if (countFilter < 0) { return -1; }

            volk_32f_x2_multiply_32f(doubledPilot, agc.out.readBuf, agc.out.readBuf, count);

            volk_32f_x2_multiply_32f(a_minus_b, decodeInput.readBuf, doubledPilot, count);

            volk_32f_x2_add_32f(a_out, decodeInput.readBuf, a_minus_b, count);
            volk_32f_x2_subtract_32f(b_out, decodeInput.readBuf, a_minus_b, count);

            decodeInput.flush();
            agc.out.flush();

            volk_32f_x2_interleave_32fc((lv_32fc_t*)out.writeBuf, a_out, b_out, count);

            if (!out.swap(count)) { return -1; }
            return count;
        }

        void start() {
            std::lock_guard<std::mutex> lck(generic_block<StereoFMDemod>::ctrlMtx);
            if (generic_block<StereoFMDemod>::running) {
                return;
            }
            generic_block<StereoFMDemod>::running = true;
            generic_block<StereoFMDemod>::doStart();
            fmDemod.start();
            split.start();
            filter.start();
            agc.start();
        }

        void stop() {
            std::lock_guard<std::mutex> lck(generic_block<StereoFMDemod>::ctrlMtx);
            if (!generic_block<StereoFMDemod>::running) {
                return;
            }
            fmDemod.stop();
            split.stop();
            filter.stop();
            agc.stop();
            generic_block<StereoFMDemod>::doStop();
            generic_block<StereoFMDemod>::running = false;
        }

        stream<stereo_t> out;

    private:
        int count;
        int countFilter;

        float _sampleRate;

        FloatFMDemod fmDemod;
        Splitter<float> split;

        // Pilot tone filtering
        stream<float> filterInput;
        FIR<float> filter;
        filter_window::BlackmanBandpassWindow win;
        AGC agc;

        stream<float> decodeInput;

        // Buffers
        float* doubledPilot;
        float* a_minus_b;
        float* a_out;
        float* b_out;

    };

    class AMDemod : public generic_block<AMDemod> {
    public:
        AMDemod() {}

        AMDemod(stream<complex_t>* in) { init(in); }

        ~AMDemod() { generic_block<AMDemod>::stop(); }

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<AMDemod>::registerInput(_in);
            generic_block<AMDemod>::registerOutput(&out);
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<AMDemod>::ctrlMtx);
            generic_block<AMDemod>::tempStop();
            generic_block<AMDemod>::unregisterInput(_in);
            _in = in;
            generic_block<AMDemod>::registerInput(_in);
            generic_block<AMDemod>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_magnitude_32f(out.writeBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();

            volk_32f_accumulator_s32f(&avg, out.writeBuf, count);
            avg /= (float)count;

            for (int i = 0; i < count; i++) {
                out.writeBuf[i] -= avg;
            }

            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        float avg;
        int count;
        stream<complex_t>* _in;

    };

    class SSBDemod : public generic_block<SSBDemod> {
    public:
        SSBDemod() {}

        SSBDemod(stream<complex_t>* in, float sampleRate, float bandWidth, int mode) { init(in, sampleRate, bandWidth, mode); }

        ~SSBDemod() {
            generic_block<SSBDemod>::stop();
            delete[] buffer;
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
        }

        void setInput(stream<complex_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<SSBDemod>::ctrlMtx);
            generic_block<SSBDemod>::tempStop();
            generic_block<SSBDemod>::unregisterInput(_in);
            _in = in;
            generic_block<SSBDemod>::registerInput(_in);
            generic_block<SSBDemod>::tempStart();
        }

        void setSampleRate(float sampleRate) {
            // No need to restart
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
            // No need to restart
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
            count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_s32fc_x2_rotator_32fc(buffer, (lv_32fc_t*)_in->readBuf, phaseDelta, &phase, count);
            volk_32fc_deinterleave_real_32f(out.writeBuf, buffer, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        int count;
        int _mode;
        float _sampleRate, _bandWidth;
        stream<complex_t>* _in;
        lv_32fc_t* buffer;
        lv_32fc_t phase;
        lv_32fc_t phaseDelta;

    };
}