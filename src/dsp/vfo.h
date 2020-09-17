#pragma once
#include <dsp/source.h>
#include <dsp/math.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <spdlog/spdlog.h>
#include <dsp/block.h>

namespace dsp {
    class VFO {
    public:
        VFO() {

        }

        void init(stream<complex_t>* in, float inputSampleRate, float outputSampleRate, float bandWidth, float offset, int blockSize) {
            _input = in;
            _outputSampleRate = outputSampleRate;
            _inputSampleRate = inputSampleRate;
            _bandWidth = bandWidth;
            _blockSize = blockSize;
            output = &resamp.output;

            lo.init(offset, inputSampleRate, blockSize);
            mixer.init(in, &lo.output, blockSize);
            //resamp.init(&mixer.output, inputSampleRate, outputSampleRate, blockSize, _bandWidth * 0.8f, _bandWidth);
            resamp.init(mixer.out[0], inputSampleRate, outputSampleRate, blockSize, _bandWidth * 0.8f, _bandWidth);
        }

        void start() {
            lo.start();
            mixer.start();
            resamp.start();
        }

        void stop(bool resampler = true) {
            lo.stop();
            mixer.stop();
            if (resampler) { resamp.stop(); };
        }

        void setInputSampleRate(float inputSampleRate, int blockSize = -1) {
            lo.stop();
            lo.setSampleRate(inputSampleRate);

            _inputSampleRate = inputSampleRate;

            if (blockSize > 0) {
                _blockSize = blockSize;
                mixer.stop();
                lo.setBlockSize(_blockSize);
                mixer.setBlockSize(_blockSize);
                mixer.start();
            }
            resamp.setInputSampleRate(inputSampleRate, _blockSize, _bandWidth * 0.8f, _bandWidth);
            lo.start();
        }

        void setOutputSampleRate(float outputSampleRate, float bandWidth = -1) {
            if (bandWidth > 0) {
                _bandWidth = bandWidth;
            }
            resamp.setOutputSampleRate(outputSampleRate, _bandWidth * 0.8f, _bandWidth);
        }

        void setBandwidth(float bandWidth) {
            _bandWidth = bandWidth;
            resamp.setFilterParams(_bandWidth * 0.8f, _bandWidth);
        }

        void setOffset(float offset) {
            lo.setFrequency(-offset);
        }

        void setBlockSize(int blockSize) {
            stop(false);
            _blockSize = blockSize;
            lo.setBlockSize(_blockSize);
            mixer.setBlockSize(_blockSize);
            resamp.setBlockSize(_blockSize);
            start();
        }

        int getOutputBlockSize() {
            return resamp.getOutputBlockSize();
        }

        stream<complex_t>* output;

    private:
        SineSource lo;
        //Multiplier mixer;
        DemoMultiplier mixer;
        FIRResampler<complex_t> resamp;
        DecimatingFIRFilter filter;
        stream<complex_t>* _input;

        float _outputSampleRate;
        float _inputSampleRate;
        float _bandWidth;
        float _blockSize;
    };
};