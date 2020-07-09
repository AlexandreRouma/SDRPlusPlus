#pragma once
#include <thread>
#include <dsp/filter.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <numeric>

#include <Windows.h>


namespace dsp {
    template <class T>
    class Interpolator {
    public:
        Interpolator() {
            
        }

        Interpolator(stream<T>* in, float interpolation, int blockSize) : output(blockSize * interpolation * 2) {
            _input = in;
            _interpolation = interpolation;
            _blockSize = blockSize;
        }

        void init(stream<T>* in, float interpolation, int blockSize) {
            output.init(blockSize * 2 * interpolation);
            _input = in;
            _interpolation = interpolation;
            _blockSize = blockSize;
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
            output.setMaxLatency(_blockSize * _interpolation * 2);
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
            output.setMaxLatency(_blockSize * _interpolation * 2);
        }

        void setInput(stream<T>* input) {
            if (running) {
                return;
            }
            _input = input;
        }

        stream<T> output;

    private:
        static void _worker(Interpolator<T>* _this) {
            T* inBuf = new T[_this->_blockSize];
            T* outBuf = new T[_this->_blockSize * _this->_interpolation];
            int outCount = _this->_blockSize * _this->_interpolation;
            while (true) {
                if (_this->_input->read(inBuf, _this->_blockSize) < 0) { break; };
                for (int i = 0; i < outCount; i++) {
                    outBuf[i] = inBuf[(int)((float)i / _this->_interpolation)];
                }
                if (_this->output.write(outBuf, outCount) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<T>* _input;
        int _blockSize;
        float _interpolation;
        std::thread _workerThread;
        bool running = false;
    };

    class BlockDecimator {
    public:
        BlockDecimator() {
            
        }

        BlockDecimator(stream<complex_t>* in, int skip, int blockSize) : output(blockSize * 2) {
            _input = in;
            _skip = skip;
            _blockSize = blockSize;
        }

        void init(stream<complex_t>* in, int skip, int blockSize) {
            output.init(blockSize * 2);
            _input = in;
            _skip = skip;
            _blockSize = blockSize;
        }

        void start() {
            if (running) {
                return;
            }
            _workerThread = std::thread(_worker, this);
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

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
            output.setMaxLatency(blockSize * 2);
        }

        void setSkip(int skip) {
            if (running) {
                return;
            }
            _skip = skip;
        }

        stream<complex_t> output;

    private:
        static void _worker(BlockDecimator* _this) {
            complex_t* buf = new complex_t[_this->_blockSize];
            while (true) {
                _this->_input->readAndSkip(buf, _this->_blockSize, _this->_skip);
                _this->output.write(buf, _this->_blockSize);
            }
        }

        stream<complex_t>* _input;
        int _blockSize;
        int _skip;
        std::thread _workerThread;
        bool running = false;
    };

    class Resampler {
    public:
        Resampler() {

        }

        void init(stream<complex_t>* in, float inputSampleRate, float outputSampleRate, float bandWidth, int blockSize) {
            _input = in;
            _outputSampleRate = outputSampleRate;
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;
            _blockSize = blockSize;
            output = &decim.output;

            dsp::BlackmanWindow(_taps, inputSampleRate * _interp, outputSampleRate / 2.0f, outputSampleRate / 2.0f);

            interp.init(in, _interp, blockSize);
            if (_interp == 1) {
                decim.init(in, _taps, blockSize, _decim);
            }
            else {
                decim.init(&interp.output, _taps, blockSize * _interp, _decim);
            }
        }

        void start() {
            if (running) {
                return;
            }
            if (_interp != 1) {
                interp.start();
            }
            decim.start();
            running = true;
        }

        void stop() {
            if (!running) {
                return;
            }
            interp.stop();
            decim.stop();
            running = false;
        }

        void setInputSampleRate(float inputSampleRate, int blockSize = -1) {
            stop();
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)_outputSampleRate);
            _interp = _outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;

            dsp::BlackmanWindow(_taps, inputSampleRate * _interp, _outputSampleRate / 2.0f, _outputSampleRate / 2.0f);
            decim.setTaps(_taps);

            interp.setInterpolation(_interp);
            decim.setDecimation(_decim);
            if (blockSize > 0) {
                _blockSize = blockSize;
                interp.setBlockSize(_blockSize);
            }
            decim.setBlockSize(_blockSize * _interp);

            if (_interp == 1) {
                decim.setInput(_input);
            }
            else {
                decim.setInput(&interp.output);
                interp.start();
            }
            start();
        }

        void setOutputSampleRate(float outputSampleRate) {
            stop();
            _outputSampleRate = outputSampleRate;
            int _gcd = std::gcd((int)_inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = _inputSampleRate / _gcd;

            dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, outputSampleRate / 2.0f, outputSampleRate / 2.0f);
            decim.setTaps(_taps);

            interp.setInterpolation(_interp);
            decim.setDecimation(_decim);
            decim.setBlockSize(_blockSize * _interp);

            if (_interp == 1) {
                decim.setInput(_input);
            }
            else {
                decim.setInput(&interp.output);
            }
            start();
        }

        void setBlockSize(int blockSize) {
            stop();
            _blockSize = blockSize;
            interp.setBlockSize(_blockSize);
            decim.setBlockSize(_blockSize * _interp);
            start();
        }

        void setInput(stream<complex_t>* input) {
            if (running) {
                return;
            }
            _input = input;
            interp.setInput(_input);
            if (_interp == 1) {
                decim.setInput(_input);
            }
        }

        stream<complex_t>* output;

    private:
        Interpolator<complex_t> interp;
        DecimatingFIRFilter decim;
        stream<complex_t>* _input;

        std::vector<float> _taps;
        int _interp;
        int _decim;
        float _outputSampleRate;
        float _inputSampleRate;
        float _blockSize;
        bool running = false;
    };



    class FloatResampler {
    public:
        FloatResampler() {

        }

        void init(stream<float>* in, float inputSampleRate, float outputSampleRate, float bandWidth, int blockSize) {
            _input = in;
            _outputSampleRate = outputSampleRate;
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;
            _blockSize = blockSize;
            output = &decim.output;

            dsp::BlackmanWindow(_taps, inputSampleRate * _interp, outputSampleRate / 2.0f, outputSampleRate / 2.0f);

            interp.init(in, _interp, blockSize);
            if (_interp == 1) {
                decim.init(in, _taps, blockSize, _decim);
            }
            else {
                decim.init(&interp.output, _taps, blockSize * _interp, _decim);
            }
        }

        void start() {
            if (running) {
                return;
            }
            if (_interp != 1) {
                interp.start();
            }
            decim.start();
            running = true;
        }

        void stop() {
            if (!running) {
                return;
            }
            interp.stop();
            //decim.stop();
            Sleep(200);
            running = false;
        }

        void setInputSampleRate(float inputSampleRate, int blockSize = -1) {
            stop();
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)_outputSampleRate);
            _interp = _outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;

            dsp::BlackmanWindow(_taps, inputSampleRate * _interp, _outputSampleRate / 2.0f, _outputSampleRate / 2.0f);
            decim.setTaps(_taps);

            interp.setInterpolation(_interp);
            decim.setDecimation(_decim);
            if (blockSize > 0) {
                _blockSize = blockSize;
                interp.setBlockSize(_blockSize);
            }
            decim.setBlockSize(_blockSize * _interp);

            if (_interp == 1) {
                decim.setInput(_input);
            }
            else {
                decim.setInput(&interp.output);
            }
            start();
        }

        void setOutputSampleRate(float outputSampleRate) {
            stop();
            _outputSampleRate = outputSampleRate;
            int _gcd = std::gcd((int)_inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = _inputSampleRate / _gcd;

            dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, outputSampleRate / 2.0f, outputSampleRate / 2.0f);
            decim.setTaps(_taps);

            interp.setInterpolation(_interp);
            decim.setDecimation(_decim);
            decim.setBlockSize(_blockSize * _interp);

            if (_interp == 1) {
                decim.setInput(_input);
            }
            else {
                decim.setInput(&interp.output);
            }
            start();
        }

        void setBlockSize(int blockSize) {
            stop();
            _blockSize = blockSize;
            interp.setBlockSize(_blockSize);
            decim.setBlockSize(_blockSize * _interp);
            start();
        }

        void setInput(stream<float>* input) {
            if (running) {
                return;
            }
            _input = input;
            interp.setInput(_input);
            if (_interp == 1) {
                decim.setInput(_input);
            }
        }

        stream<float>* output;

    private:
        Interpolator<float> interp;
        FloatDecimatingFIRFilter decim;
        stream<float>* _input;

        std::vector<float> _taps;
        int _interp;
        int _decim;
        float _outputSampleRate;
        float _inputSampleRate;
        float _blockSize;
        bool running = false;
    };
    
    
};