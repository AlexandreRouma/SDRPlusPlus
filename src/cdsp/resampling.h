#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>
#include <cdsp/filter.h>
#include <numeric>


namespace cdsp {
    class Interpolator {
    public:
        Interpolator() {
            
        }

        Interpolator(stream<float>* in, float interpolation, int bufferSize) : output(bufferSize * 2) {
            _input = in;
            _interpolation = interpolation;
            _bufferSize = bufferSize;
            running = false;
        }

        void init(stream<float>* in, float interpolation, int bufferSize) {
            output.init(bufferSize * 2);
            _input = in;
            _interpolation = interpolation;
            _bufferSize = bufferSize;
            running = false;
        }

        void start() {
            if (running) {
                return;
            }
            running = true;
            _workerThread = std::thread(_worker, this);
        }

        void stop() {
            if (!running) {
                return;
            }
            running = false;
            _input->stopReader();
            output.stopWriter();
            _workerThread.join();
            _input->clearReadStop();
            output.clearWriteStop();
        }

        void setInterpolation(float interpolation) {
            if (running) {
                return;
            }
            _interpolation = interpolation;
        }

        void setInput(stream<float>* in) {
            if (running) {
                return;
            }
            _input = in;
        }

        stream<float> output;

    private:
        static void _worker(Interpolator* _this) {
            float* inBuf = new float[(int)((float)_this->_bufferSize / _this->_interpolation)];
            float* outBuf = new float[_this->_bufferSize];
            while (true) {
                if (_this->_input->read(inBuf, (int)((float)_this->_bufferSize / _this->_interpolation)) < 0) { break; };
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i] = inBuf[(int)((float)i / _this->_interpolation)];
                }
                if (_this->output.write(outBuf, _this->_bufferSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<float>* _input;
        int _bufferSize;
        float _interpolation;
        std::thread _workerThread;
        bool running;
    };


    class IQInterpolator {
    public:
        IQInterpolator() {
            
        }

        IQInterpolator(stream<complex_t>* in, float interpolation, int bufferSize) : output(bufferSize * 2) {
            _input = in;
            _interpolation = interpolation;
            _bufferSize = bufferSize;
            running = false;
        }

        void init(stream<complex_t>* in, float interpolation, int bufferSize) {
            output.init(bufferSize * 2);
            _input = in;
            _interpolation = interpolation;
            _bufferSize = bufferSize;
            running = false;
        }

        void start() {
            if (running) {
                return;
            }
            _workerThread = std::thread(_worker, this);
            running = true;
        }

        void stop() {
            if (!running) {
                return;
            }
            _input->stopReader();
            output.stopWriter();
            _workerThread.join();
            _input->clearReadStop();
            output.clearWriteStop();
            running = false;
        }

        void setInterpolation(float interpolation) {
            if (running) {
                return;
            }
            _interpolation = interpolation;
        }

        stream<complex_t> output;

    private:
        static void _worker(IQInterpolator* _this) {
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            complex_t* outBuf = new complex_t[_this->_bufferSize * _this->_interpolation];
            int outCount = _this->_bufferSize * _this->_interpolation;
            while (true) {
                if (_this->_input->read(inBuf, _this->_bufferSize) < 0) { break; };
                for (int i = 0; i < outCount; i++) {
                    outBuf[i] = inBuf[(int)((float)i / _this->_interpolation)];
                }
                if (_this->output.write(outBuf, _this->_bufferSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<complex_t>* _input;
        int _bufferSize;
        float _interpolation;
        std::thread _workerThread;
        bool running;
    };

    class BlockDecimator {
    public:
        BlockDecimator() {
            
        }

        BlockDecimator(stream<complex_t>* in, int skip, int bufferSize) : output(bufferSize * 2) {
            _input = in;
            _skip = skip;
            _bufferSize = bufferSize;
        }

        void init(stream<complex_t>* in, int skip, int bufferSize) {
            output.init(bufferSize * 2);
            _input = in;
            _skip = skip;
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output;

    private:
        static void _worker(BlockDecimator* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            while (true) {
                _this->_input->readAndSkip(buf, _this->_bufferSize, _this->_skip);
                _this->output.write(buf, _this->_bufferSize);
            }
        }

        stream<complex_t>* _input;
        int _bufferSize;
        int _skip;
        std::thread _workerThread;
    };

    class FractionalResampler {
    public:
        FractionalResampler() {

        }

        void init(stream<float>* input, float inputSampleRate, float outputSampleRate, int bufferSize, float customCutoff = INFINITY) {
            _input = input;
            float lowestFreq = std::min<float>(inputSampleRate, outputSampleRate);
            int _gcd = std::gcd((int)inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;
            _inputSampleRate = inputSampleRate;
            _outputSampleRate = outputSampleRate;
            running = false;

            interpolator.init(input, _interp, bufferSize);

            BlackmanWindow(decimTaps, inputSampleRate * _interp, lowestFreq / 2.0f, lowestFreq / 2.0f);

            if (_interp != 1) {
                printf("FR Interpolation needed\n");
                decimator.init(&interpolator.output, decimTaps, bufferSize * _interp, _decim);
            }
            else {
                decimator.init(input, decimTaps, bufferSize, _decim);
                printf("FR Interpolation NOT needed: %d %d %d\n", bufferSize / _decim, _decim, _interp);
            }

            output = &decimator.output;
        }

        void start() {
            if (_interp != 1) {
                interpolator.start();
            }
            decimator.start();
            running = true;
        }

        void stop() {
            interpolator.stop();
            decimator.stop();
            running = false;
        }

        void setInputSampleRate(float inputSampleRate) {
            if (running) {
                return;
            }
            float lowestFreq = std::min<float>(inputSampleRate, _outputSampleRate);
            int _gcd = std::gcd((int)inputSampleRate, (int)_outputSampleRate);
            _interp = _outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;

            // TODO: Add checks from VFO to remove the need to stop both
            interpolator.setInterpolation(_interp);
            decimator.setDecimation(_decim);
            if (_interp != 1) {
                decimator.setInput(&interpolator.output);
            }
            else {
                decimator.setInput(_input);
            }
        }

        void setOutputSampleRate(float outputSampleRate) {
            if (running) {
                return;
            }
            float lowestFreq = std::min<float>(_inputSampleRate, outputSampleRate);
            int _gcd = std::gcd((int)_inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = _inputSampleRate / _gcd;

            // TODO: Add checks from VFO to remove the need to stop both
            interpolator.setInterpolation(_interp);
            decimator.setDecimation(_decim);
            if (_interp != 1) {
                decimator.setInput(&interpolator.output);
            }
            else {
                decimator.setInput(_input);
            }
        }

        void setInput(stream<float>* input) {
            if (running) {
                return;
            }
            _input = input;
            if (_interp != 1) {
                interpolator.setInput(input);
                decimator.setInput(&interpolator.output);
            }
            else {
                decimator.setInput(input);
            }
        }

        stream<float>* output;

    private:
        Interpolator interpolator;
        FloatDecimatingFIRFilter decimator;
        std::vector<float> decimTaps;
        stream<float>* _input;
        int _interp;
        int _decim;
        int _bufferSize;
        float _inputSampleRate;
        float _outputSampleRate;
        bool running;
    };
};