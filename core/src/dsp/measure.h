#pragma once
#include <dsp/block.h>
#include <fftw3.h>
#include <volk/volk.h>
#include <spdlog/spdlog.h>
#include <dsp/types.h>
#include <string.h>

namespace dsp {
    class VolumeMeasure : public generic_block<VolumeMeasure> {
    public:
        VolumeMeasure() {}

        VolumeMeasure(stream<stereo_t>* in) { init(in); }

        ~VolumeMeasure() {
            generic_block<VolumeMeasure>::stop();
            delete[] leftBuf;
            delete[] rightBuf;
        }

        void init(stream<stereo_t>* in) {
            _in = in;
            leftBuf = new float[STREAM_BUFFER_SIZE];
            rightBuf = new float[STREAM_BUFFER_SIZE];
            generic_block<VolumeMeasure>::registerInput(_in);
            generic_block<VolumeMeasure>::registerOutput(&out);
        }

        void setInput(stream<stereo_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<VolumeMeasure>::ctrlMtx);
            generic_block<VolumeMeasure>::tempStop();
            generic_block<VolumeMeasure>::unregisterInput(_in);
            _in = in;
            generic_block<VolumeMeasure>::registerInput(_in);
            generic_block<VolumeMeasure>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            memcpy(out.writeBuf, _in->readBuf, count * sizeof(stereo_t));
            volk_32fc_deinterleave_32f_x2(leftBuf, rightBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }

            // Get peak from last value
            float time = (float)count / sampleRate;
            peak.l -= peakFall * time;
            peak.r -= peakFall * time;
            stereo_t _peak;
            _peak.l = powf(10, peak.l / 10.0f);
            _peak.r = powf(10, peak.r / 10.0f);

            stereo_t _average;

            // Calculate average
            volk_32f_s32f_power_32f(leftBuf, leftBuf, 2, count);
            volk_32f_s32f_power_32f(rightBuf, rightBuf, 2, count);
            volk_32f_sqrt_32f(leftBuf, leftBuf, count);
            volk_32f_sqrt_32f(rightBuf, rightBuf, count);
            volk_32f_accumulator_s32f(&_average.l, leftBuf, count);
            volk_32f_accumulator_s32f(&_average.r, rightBuf, count);
            _average.l /= (float)count;
            _average.r /= (float)count;

            // Calculate peak
            for (int i = 0; i < count; i++) {
                if (leftBuf[i] > _peak.l) { _peak.l = leftBuf[i]; }
                if (rightBuf[i] > _peak.r) { _peak.r = rightBuf[i]; }
            }

            // Assign
            peak.l = 10.0f * log10f(_peak.l);
            peak.r = 10.0f * log10f(_peak.r);
            average.l = (average.l * (1.0f - avgFilt)) + (10.0f * log10f(_average.l) * avgFilt);
            average.r = (average.r * (1.0f - avgFilt)) + (10.0f * log10f(_average.r) * avgFilt);

            return count;
        }

        stream<stereo_t> out;

        stereo_t peak = {0, 0};
        stereo_t average = {0, 0};

    private:
        int count;
        float peakFall = 10.0f; // dB/S
        float avgFilt  = 0.2f; // IIR filter coef
        float sampleRate = 48000;
        stream<stereo_t>* _in;

        float* leftBuf;
        float* rightBuf;
    };
}