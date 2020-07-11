#pragma once
#include <dsp/source.h>
#include <dsp/math.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>

namespace dsp {
    class VFO {
    public:
        VFO() {

        }

        void init(stream<complex_t>* in, float inputSampleRate, float outputSampleRate, float bandWidth, float offset, int blockSize) {
            _input = in;
            _outputSampleRate = outputSampleRate;
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;
            _bandWidth = bandWidth;
            _blockSize = blockSize;
            output = &decim.output;

            dsp::BlackmanWindow(_taps, inputSampleRate * _interp, bandWidth / 2.0f, bandWidth / 2.0f);

            lo.init(offset, inputSampleRate, blockSize);
            mixer.init(in, &lo.output, blockSize);
            interp.init(&mixer.output, _interp, blockSize);
            if (_interp == 1) {
                decim.init(&mixer.output, _taps, blockSize, _decim);
            }
            else {
                decim.init(&interp.output, _taps, blockSize * _interp, _decim);
            }
        }

        void start() {
            lo.start();
            mixer.start();
            if (_interp != 1) {
                interp.start();
            }
            decim.start();
        }

        void stop() {
            lo.stop();
            mixer.stop();
            interp.stop();
            decim.stop();
        }

        void setInputSampleRate(float inputSampleRate, int blockSize = -1) {
            interp.stop();
            decim.stop();

            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)_outputSampleRate);
            _interp = _outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;

            dsp::BlackmanWindow(_taps, inputSampleRate * _interp, _bandWidth / 2.0f, _bandWidth / 2.0f);

            interp.setInterpolation(_interp);
            decim.setDecimation(_decim);
            if (blockSize > 0) {
                lo.stop();
                mixer.stop();
                _blockSize = blockSize;
                lo.setBlockSize(_blockSize);
                mixer.setBlockSize(_blockSize);
                interp.setBlockSize(_blockSize);
                lo.start();
                mixer.start();
            }
            decim.setBlockSize(_blockSize * _interp);

            if (_interp == 1) {
                decim.setInput(&mixer.output);
            }
            else {
                decim.setInput(&interp.output);
                interp.start();
            }
            decim.start();
        }

        void setOutputSampleRate(float outputSampleRate, float bandWidth = -1) {
            interp.stop();
            decim.stop();

            if (bandWidth > 0) {
                _bandWidth = bandWidth;
            }
            
            _outputSampleRate = outputSampleRate;
            int _gcd = std::gcd((int)_inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = _inputSampleRate / _gcd;

            dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, _bandWidth / 2.0f, _bandWidth / 2.0f);
            decim.setTaps(_taps);

            interp.setInterpolation(_interp);
            decim.setDecimation(_decim);
            decim.setBlockSize(_blockSize * _interp);

            if (_interp == 1) {
                decim.setInput(&mixer.output);
            }
            else {
                decim.setInput(&interp.output);
                interp.start();
            }
            decim.start();
        }

        void setBandwidth(float bandWidth) {
            decim.stop();
            dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, _bandWidth / 2.0f, _bandWidth / 2.0f);
            decim.setTaps(_taps);
            decim.start();
        }

        void setOffset(float offset) {
            lo.setFrequency(-offset);
        }

        void setBlockSize(int blockSize) {
            stop();
            _blockSize = blockSize;
            lo.setBlockSize(_blockSize);
            mixer.setBlockSize(_blockSize);
            interp.setBlockSize(_blockSize);
            decim.setBlockSize(_blockSize * _interp);
            start();
        }

        stream<complex_t>* output;

    private:
        SineSource lo;
        Multiplier mixer;
        Interpolator<complex_t> interp;
        DecimatingFIRFilter decim;
        stream<complex_t>* _input;

        std::vector<float> _taps;
        int _interp;
        int _decim;
        float _outputSampleRate;
        float _inputSampleRate;
        float _bandWidth;
        float _blockSize;
    };
};