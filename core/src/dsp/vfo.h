#pragma once
#include <dsp/block.h>
#include <dsp/window.h>
#include <dsp/resampling.h>
#include <dsp/processing.h>
#include <algorithm>

namespace dsp {
    class VFO {
    public:
        VFO() {}

        ~VFO() { stop(); }

        VFO(stream<complex_t>* in, float offset, float inSampleRate, float outSampleRate, float bandWidth) {
            init(in, offset, inSampleRate, outSampleRate, bandWidth);
        };

        void init(stream<complex_t>* in, float offset, float inSampleRate, float outSampleRate, float bandWidth) {
            _in = in;
            _offset = offset;
            _inSampleRate = inSampleRate;
            _outSampleRate = outSampleRate;
            _bandWidth = bandWidth;

            float realCutoff = std::min<float>(_bandWidth, std::min<float>(_inSampleRate, _outSampleRate)) / 2.0f;

            xlator.init(_in, _inSampleRate, -_offset);
            win.init(realCutoff, realCutoff, inSampleRate);
            resamp.init(&xlator.out, &win, _inSampleRate, _outSampleRate);

            win.setSampleRate(_inSampleRate * resamp.getInterpolation());
            resamp.updateWindow(&win);

            out = &resamp.out;
        }

        void start() {
            if (running) { return; }
            xlator.start();
            resamp.start();
        }

        void stop() {
            if (!running) { return; }
            xlator.stop();
            resamp.stop();
        }

        void setInSampleRate(float inSampleRate) {
            _inSampleRate = inSampleRate;
            if (running) { xlator.stop(); resamp.stop(); }
            xlator.setSampleRate(_inSampleRate);
            resamp.setInSampleRate(_inSampleRate);
            float realCutoff = std::min<float>(_bandWidth, std::min<float>(_inSampleRate, _outSampleRate)) / 2.0f;
            win.setSampleRate(_inSampleRate * resamp.getInterpolation());
            win.setCutoff(realCutoff);
            win.setTransWidth(realCutoff);
            resamp.updateWindow(&win);
            if (running) { xlator.start(); resamp.start(); }
        }

        void setOutSampleRate(float outSampleRate) {
            _outSampleRate = outSampleRate;
            if (running) { resamp.stop(); }
            resamp.setOutSampleRate(_outSampleRate);
            float realCutoff = std::min<float>(_bandWidth, std::min<float>(_inSampleRate, _outSampleRate)) / 2.0f;
            win.setSampleRate(_inSampleRate * resamp.getInterpolation());
            win.setCutoff(realCutoff);
            win.setTransWidth(realCutoff);
            resamp.updateWindow(&win);
            if (running) { resamp.start(); }
        }

        void setOutSampleRate(float outSampleRate, float bandWidth) {
            _outSampleRate = outSampleRate;
            _bandWidth = bandWidth;
            if (running) { resamp.stop(); }
            resamp.setOutSampleRate(_outSampleRate);
            float realCutoff = std::min<float>(_bandWidth, std::min<float>(_inSampleRate, _outSampleRate)) / 2.0f;
            win.setSampleRate(_inSampleRate * resamp.getInterpolation());
            win.setCutoff(realCutoff);
            win.setTransWidth(realCutoff);
            resamp.updateWindow(&win);
            if (running) { resamp.start(); }
        }

        void setOffset(float offset) {
            _offset = offset;
            xlator.setFrequency(-_offset);
        }

        void setBandwidth(float bandWidth) {
            _bandWidth = bandWidth;
            float realCutoff = std::min<float>(_bandWidth, std::min<float>(_inSampleRate, _outSampleRate)) / 2.0f;
            win.setCutoff(realCutoff);
            win.setTransWidth(realCutoff);
            resamp.updateWindow(&win);
        }

        stream<complex_t>* out;

    private:
        bool running = false;
        float _offset, _inSampleRate, _outSampleRate, _bandWidth;
        filter_window::BlackmanWindow win;
        stream<complex_t>* _in;
        FrequencyXlator xlator;
        PolyphaseResampler<complex_t> resamp;

    };
}