#pragma once
#include <dsp/block.h>
#include <dsp/stream.h>
#include <dsp/types.h>

namespace dsp {

    class FMStereoDemuxPilotFilter : public generic_block<FMStereoDemuxPilotFilter> {
    public:
        FMStereoDemuxPilotFilter() {}

        FMStereoDemuxPilotFilter(stream<complex_t>* in, dsp::filter_window::generic_complex_window* window) { init(in, window); }

        ~FMStereoDemuxPilotFilter() {
            if (!generic_block<FMStereoDemuxPilotFilter>::_block_init) { return; }
            generic_block<FMStereoDemuxPilotFilter>::stop();
            volk_free(buffer);
            volk_free(taps);
            generic_block<FMStereoDemuxPilotFilter>::_block_init = false;
        }

        void init(stream<complex_t>* in, dsp::filter_window::generic_complex_window* window) {
            _in = in;

            tapCount = window->getTapCount();
            taps = (complex_t*)volk_malloc(tapCount * sizeof(complex_t), volk_get_alignment());
            window->createTaps(taps, tapCount);

            buffer = (complex_t*)volk_malloc(STREAM_BUFFER_SIZE * sizeof(complex_t) * 2, volk_get_alignment());
            bufStart = &buffer[tapCount];
            generic_block<FMStereoDemuxPilotFilter>::registerInput(_in);
            generic_block<FMStereoDemuxPilotFilter>::registerOutput(&dataOut);
            generic_block<FMStereoDemuxPilotFilter>::registerOutput(&pilotOut);
            generic_block<FMStereoDemuxPilotFilter>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<FMStereoDemuxPilotFilter>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FMStereoDemuxPilotFilter>::ctrlMtx);
            generic_block<FMStereoDemuxPilotFilter>::tempStop();
            generic_block<FMStereoDemuxPilotFilter>::unregisterInput(_in);
            _in = in;
            generic_block<FMStereoDemuxPilotFilter>::registerInput(_in);
            generic_block<FMStereoDemuxPilotFilter>::tempStart();
        }

        void updateWindow(dsp::filter_window::generic_complex_window* window) {
            assert(generic_block<FMStereoDemuxPilotFilter>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FMStereoDemuxPilotFilter>::ctrlMtx);
            _window = window;
            volk_free(taps);
            tapCount = window->getTapCount();
            taps = (complex_t*)volk_malloc(tapCount * sizeof(complex_t), volk_get_alignment());
            bufStart = &buffer[tapCount];
            window->createTaps(taps, tapCount);
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            generic_block<FMStereoDemuxPilotFilter>::ctrlMtx.lock();

            memcpy(bufStart, _in->readBuf, count * sizeof(complex_t));
            _in->flush();

            for (int i = 0; i < count; i++) {
                volk_32fc_x2_dot_prod_32fc((lv_32fc_t*)&pilotOut.writeBuf[i], (lv_32fc_t*)&buffer[i+1], (lv_32fc_t*)taps, tapCount);
            }

            memcpy(dataOut.writeBuf, &buffer[tapCount - ((tapCount-1)/2)], count * sizeof(complex_t));

            if (!dataOut.swap(count)) { return -1; }
            if (!pilotOut.swap(count)) { return -1; }

            memmove(buffer, &buffer[count], tapCount * sizeof(complex_t));

            generic_block<FMStereoDemuxPilotFilter>::ctrlMtx.unlock();

            return count;
        }

        stream<complex_t> dataOut;
        stream<complex_t> pilotOut;
        

    private:
        stream<complex_t>* _in;

        dsp::filter_window::generic_complex_window* _window;

        complex_t* bufStart;
        complex_t* buffer;
        int tapCount;
        complex_t* taps;

    };

    class FMStereoDemux: public generic_block<FMStereoDemux> {
    public:
        FMStereoDemux() {}

        FMStereoDemux(stream<complex_t>* data, stream<complex_t>* pilot, float loopBandwidth) { init(data, pilot, loopBandwidth); }

        void init(stream<complex_t>* data, stream<complex_t>* pilot, float loopBandwidth) {
            _data = data;
            _pilot = pilot;
            lastVCO.re = 1.0f;
            lastVCO.im = 0.0f;
            _loopBandwidth = loopBandwidth;

            float dampningFactor = sqrtf(2.0f) / 2.0f;
            float denominator = (1.0 + 2.0 * dampningFactor * _loopBandwidth + _loopBandwidth * _loopBandwidth);
            _alpha = (4 * dampningFactor * _loopBandwidth) / denominator;
            _beta = (4 * _loopBandwidth * _loopBandwidth) / denominator;

            generic_block<FMStereoDemux>::registerInput(_data);
            generic_block<FMStereoDemux>::registerInput(_pilot);
            generic_block<FMStereoDemux>::registerOutput(&AplusBOut);
            generic_block<FMStereoDemux>::registerOutput(&AminusBOut);
            generic_block<FMStereoDemux>::_block_init = true;
        }

        void setInput(stream<complex_t>* data, stream<complex_t>* pilot) {
            assert(generic_block<FMStereoDemux>::_block_init);
            generic_block<FMStereoDemux>::tempStop();
            generic_block<FMStereoDemux>::unregisterInput(_data);
            generic_block<FMStereoDemux>::unregisterInput(_pilot);
            _data = data;
            _pilot = pilot;
            generic_block<FMStereoDemux>::registerInput(_data);
            generic_block<FMStereoDemux>::registerInput(_pilot);
            generic_block<FMStereoDemux>::tempStart();
        }

        void setLoopBandwidth(float loopBandwidth) {
            assert(generic_block<FMStereoDemux>::_block_init);
            generic_block<FMStereoDemux>::tempStop();
            _loopBandwidth = loopBandwidth;
            float dampningFactor = sqrtf(2.0f) / 2.0f;
            float denominator = (1.0 + 2.0 * dampningFactor * _loopBandwidth + _loopBandwidth * _loopBandwidth);
            _alpha = (4 * dampningFactor * _loopBandwidth) / denominator;
            _beta = (4 * _loopBandwidth * _loopBandwidth) / denominator;
            generic_block<FMStereoDemux>::tempStart();
        }

        int run() {
            int count = _data->read();
            if (count < 0) { return -1; }
            int pCount = _pilot->read();
            if (pCount < 0) { return -1; }

            complex_t doubledVCO;
            float error;

            volk_32fc_deinterleave_real_32f(AplusBOut.writeBuf, (lv_32fc_t*)_data->readBuf, count);

            for (int i = 0; i < count; i++) {
                // Double the VCO, then mix it with the input data.
                doubledVCO = lastVCO*lastVCO;
                AminusBOut.writeBuf[i] = (_data->readBuf[i].re * doubledVCO.re) + (_data->readBuf[i].im * doubledVCO.im);

                // Calculate the phase error estimation
                error = _pilot->readBuf[i].phase() - vcoPhase;
                if (error > 3.1415926535f)        { error -= 2.0f * 3.1415926535f; }
                else if (error <= -3.1415926535f) { error += 2.0f * 3.1415926535f; }
                
                // Integrate frequency and clamp it
                vcoFrequency += _beta * error;
                if (vcoFrequency > upperLimit) { vcoFrequency = upperLimit; }
                else if (vcoFrequency < lowerLimit) { vcoFrequency = lowerLimit; }

                // Calculate new phase and wrap it
                vcoPhase += vcoFrequency + (_alpha * error);
                while (vcoPhase > (2.0f * FL_M_PI)) { vcoPhase -= (2.0f * FL_M_PI); }
                while (vcoPhase < (-2.0f * FL_M_PI)) { vcoPhase += (2.0f * FL_M_PI); }

                // Calculate output
                lastVCO.re = cosf(vcoPhase);
                lastVCO.im = sinf(vcoPhase);
            }
            
            _data->flush();
            _pilot->flush();

            if (!AplusBOut.swap(count)) { return -1; }
            if (!AminusBOut.swap(count)) { return -1; }
            return count;
        }

        stream<float> AplusBOut;
        stream<float> AminusBOut;

    private:
        float _loopBandwidth = 0.01f;

        const float expectedFreq = ((19000.0f / 250000.0f) * 2.0f * FL_M_PI);
        const float upperLimit = ((19200.0f / 250000.0f) * 2.0f * FL_M_PI);
        const float lowerLimit = ((18800.0f / 250000.0f) * 2.0f * FL_M_PI);

        float _alpha; // Integral coefficient
        float _beta; // Proportional coefficient
        float vcoFrequency = expectedFreq;
        float vcoPhase = 0.0f;
        complex_t lastVCO;

        stream<complex_t>* _data;
        stream<complex_t>* _pilot;
    };

    class FMStereoReconstruct : public generic_block<FMStereoReconstruct> {
    public:
        FMStereoReconstruct() {}

        FMStereoReconstruct(stream<float>* a, stream<float>* b) { init(a, b); }

        ~FMStereoReconstruct() {
            generic_block<FMStereoReconstruct>::stop();
            delete[] leftBuf;
            delete[] rightBuf;
        }

        void init(stream<float>* aplusb, stream<float>* aminusb) {
            _aplusb = aplusb;
            _aminusb = aminusb;

            leftBuf = new float[STREAM_BUFFER_SIZE];
            rightBuf = new float[STREAM_BUFFER_SIZE];

            generic_block<FMStereoReconstruct>::registerInput(aplusb);
            generic_block<FMStereoReconstruct>::registerInput(aminusb);
            generic_block<FMStereoReconstruct>::registerOutput(&out);
            generic_block<FMStereoReconstruct>::_block_init = true;
        }

        void setInputs(stream<float>* aplusb, stream<float>* aminusb) {
            assert(generic_block<FMStereoReconstruct>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<FMStereoReconstruct>::ctrlMtx);
            generic_block<FMStereoReconstruct>::tempStop();
            generic_block<FMStereoReconstruct>::unregisterInput(_aplusb);
            generic_block<FMStereoReconstruct>::unregisterInput(_aminusb);
            _aplusb = aplusb;
            _aminusb = aminusb;
            generic_block<FMStereoReconstruct>::registerInput(_aplusb);
            generic_block<FMStereoReconstruct>::registerInput(_aminusb);
            generic_block<FMStereoReconstruct>::tempStart();
        }

        int run() {
            int a_count = _aplusb->read();
            if (a_count < 0) { return -1; }
            int b_count = _aminusb->read();
            if (b_count < 0) { return -1; }
            if (a_count != b_count) {
                _aplusb->flush();
                _aminusb->flush();
                return 0;
            }

            volk_32f_x2_add_32f(leftBuf, _aplusb->readBuf, _aminusb->readBuf, a_count);
            volk_32f_x2_subtract_32f(rightBuf, _aplusb->readBuf, _aminusb->readBuf, a_count);
            _aplusb->flush();
            _aminusb->flush();

            volk_32f_x2_interleave_32fc((lv_32fc_t*)out.writeBuf, leftBuf, rightBuf, a_count);

            if (!out.swap(a_count)) { return -1; }
            return a_count;
        }

        stream<stereo_t> out;

    private:
        stream<float>* _aplusb;
        stream<float>* _aminusb;

        float* leftBuf;
        float* rightBuf;

    };
}