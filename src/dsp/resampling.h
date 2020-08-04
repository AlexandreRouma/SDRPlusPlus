#pragma once
#include <thread>
#include <dsp/filter.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <numeric>
#include <algorithm>

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
            int interp = _this->_interpolation;
            int count = 0;
            while (true) {
                if (_this->_input->read(inBuf, _this->_blockSize) < 0) { break; };
                for (int i = 0; i < outCount; i++) {
                    outBuf[i] = inBuf[(int)((float)i / _this->_interpolation)];
                }

                // for (int i = 0; i < outCount; i += interp) {
                //     outBuf[i] = inBuf[count];
                //     count++;
                // }

                count = 0;
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

        void setBlockSize(int blockSize) {
            printf("%d\n", blockSize);
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
            bool delay = _this->_skip < 0;

            int readCount = std::min<int>(_this->_blockSize + _this->_skip, _this->_blockSize);
            int skip = std::max<int>(_this->_skip, 0);
            int delaySize = (-_this->_skip) * sizeof(complex_t);

            complex_t* start = &buf[std::max<int>(-_this->_skip, 0)];
            complex_t* delayStart = &buf[_this->_blockSize + _this->_skip];

            while (true) {
                if (delay) {
                    memmove(buf, delayStart, delaySize);
                }
                if (_this->_input->readAndSkip(start, readCount, skip) < 0) { break; };
                if (_this->output.write(buf, _this->_blockSize) < 0) { break; };
            }
            delete[] buf;
        }

        stream<complex_t>* _input;
        int _blockSize;
        int _skip;
        std::thread _workerThread;
        bool running = false;
    };

    class FIRResampler {
    public:
        FIRResampler() {

        }

        void init(stream<complex_t>* in, float inputSampleRate, float outputSampleRate, int blockSize, float passBand = -1.0f, float transWidth = -1.0f) {
            _input = in;
            _outputSampleRate = outputSampleRate;
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;
            _blockSize = blockSize;
            outputBlockSize = (blockSize * _interp) / _decim;
            output.init(outputBlockSize * 2);

            float cutoff = std::min<float>(_outputSampleRate / 2.0f, _inputSampleRate / 2.0f);
            if (passBand > 0.0f && transWidth > 0.0f) {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            }
            else {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, cutoff, cutoff);
            }
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

        void setInputSampleRate(float inputSampleRate, int blockSize = -1, float passBand = -1.0f, float transWidth = -1.0f) {
            stop();
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)_outputSampleRate);
            _interp = _outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;

            printf("FIRResampler.setInputSampleRate(): %d %d\n", _interp, _decim);

            float cutoff = std::min<float>(_outputSampleRate / 2.0f, _inputSampleRate / 2.0f);
            if (passBand > 0.0f && transWidth > 0.0f) {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            }
            else {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, cutoff, cutoff);
            }

            if (blockSize > 0) {
                _blockSize = blockSize;
            }
            outputBlockSize = (_blockSize * _interp) / _decim;
            output.setMaxLatency(outputBlockSize * 2);
            start();
        }

        void setOutputSampleRate(float outputSampleRate, float passBand = -1.0f, float transWidth = -1.0f) {
            stop();
            _outputSampleRate = outputSampleRate;
            int _gcd = std::gcd((int)_inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = _inputSampleRate / _gcd;
            outputBlockSize = (_blockSize * _interp) / _decim;
            output.setMaxLatency(outputBlockSize * 2);

            printf("FIRResampler.setOutputSampleRate(): %d %d\n", _interp, _decim);

            float cutoff = std::min<float>(_outputSampleRate / 2.0f, _inputSampleRate / 2.0f);
            if (passBand > 0.0f && transWidth > 0.0f) {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            }
            else {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, cutoff, cutoff);
            }
            
            start();
        }

        void setFilterParams(float passBand, float transWidth) {
            stop();
            dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            start();
        }

        void setBlockSize(int blockSize) {
            stop();
            _blockSize = blockSize;
            outputBlockSize = (_blockSize * _interp) / _decim;
            output.setMaxLatency(outputBlockSize * 2);
            start();
        }

        void setInput(stream<complex_t>* input) {
            if (running) {
                return;
            }
            _input = input;
        }

        int getOutputBlockSize() {
            return outputBlockSize;
        }

        stream<complex_t> output;

    private:
        static void _worker(FIRResampler* _this) {
            complex_t* inBuf = new complex_t[_this->_blockSize];
            complex_t* outBuf = new complex_t[_this->outputBlockSize];
            
            int inCount = _this->_blockSize;
            int outCount = _this->outputBlockSize;
            
            float* taps = _this->_taps.data(); 
            int tapCount = _this->_taps.size();
            complex_t* delayBuf = new complex_t[tapCount];

            complex_t* delayStart = &inBuf[std::max<int>(inCount - tapCount, 0)];
            int delaySize = tapCount * sizeof(complex_t);
            complex_t* delayBufEnd = &delayBuf[std::max<int>(tapCount - inCount, 0)];
            int moveSize = std::min<int>(inCount, tapCount - inCount) * sizeof(complex_t);
            int inSize = inCount * sizeof(complex_t);

            int interp = _this->_interp;
            int decim = _this->_decim;

            float correction = (float)sqrt((float)interp);

            printf("Resamp: %d %d", inCount, _this->outputBlockSize);
            
            int afterInterp = inCount * interp;
            int outIndex = 0;
            while (true) {q
                if (_this->_input->read(inBuf, inCount) < 0) { break; };
                for (int i = 0; outIndex < outCount; i += decim) {
                    outBuf[outIndex].i = 0;
                    outBuf[outIndex].q = 0;
                    for (int j = 0; j < tapCount; j++) {
                        if ((i - j) % interp != 0) {
                            continue;
                        }
                        outBuf[outIndex].i += GET_FROM_RIGHT_BUF(inBuf, delayBuf, tapCount, (i - j) / interp).i * taps[j] * correction;
                        outBuf[outIndex].q += GET_FROM_RIGHT_BUF(inBuf, delayBuf, tapCount, (i - j) / interp).q * taps[j] * correction;
                    }
                    outIndex++;
                }
                outIndex = 0;
                if (tapCount > inCount) {
                    memmove(delayBuf, delayBufEnd, moveSize);
                    memcpy(delayBufEnd, delayStart, inSize);
                }
                else {
                    memcpy(delayBuf, delayStart, delaySize);
                }
                
                if (_this->output.write(outBuf, _this->outputBlockSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
            delete[] delayBuf;
        }

        std::thread _workerThread;

        stream<complex_t>* _input;
        std::vector<float> _taps;
        int _interp;
        int _decim;
        int outputBlockSize;
        float _outputSampleRate;
        float _inputSampleRate;
        int _blockSize;
        bool running = false;
    };





    class FloatFIRResampler {
    public:
        FloatFIRResampler() {

        }

        void init(stream<float>* in, float inputSampleRate, float outputSampleRate, int blockSize, float passBand = -1.0f, float transWidth = -1.0f) {
            _input = in;
            _outputSampleRate = outputSampleRate;
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;
            _blockSize = blockSize;
            outputBlockSize = (blockSize * _interp) / _decim;
            output.init(outputBlockSize * 2);

            float cutoff = std::min<float>(_outputSampleRate / 2.0f, _inputSampleRate / 2.0f);
            if (passBand > 0.0f && transWidth > 0.0f) {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            }
            else {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, cutoff, cutoff);
            }
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

        void setInputSampleRate(float inputSampleRate, int blockSize = -1, float passBand = -1.0f, float transWidth = -1.0f) {
            stop();
            _inputSampleRate = inputSampleRate;
            int _gcd = std::gcd((int)inputSampleRate, (int)_outputSampleRate);
            _interp = _outputSampleRate / _gcd;
            _decim = inputSampleRate / _gcd;

            printf("FloatFIRResampler.setInputSampleRate(): %d %d\n", _interp, _decim);

            float cutoff = std::min<float>(_outputSampleRate / 2.0f, _inputSampleRate / 2.0f);
            if (passBand > 0.0f && transWidth > 0.0f) {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            }
            else {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, cutoff, cutoff);
            }

            if (blockSize > 0) {
                _blockSize = blockSize;
            }
            outputBlockSize = (blockSize * _interp) / _decim;
            output.setMaxLatency(outputBlockSize * 2);
            start();
        }

        void setOutputSampleRate(float outputSampleRate, float passBand = -1.0f, float transWidth = -1.0f) {
            stop();
            _outputSampleRate = outputSampleRate;
            int _gcd = std::gcd((int)_inputSampleRate, (int)outputSampleRate);
            _interp = outputSampleRate / _gcd;
            _decim = _inputSampleRate / _gcd;
            outputBlockSize = (_blockSize * _interp) / _decim;
            output.setMaxLatency(outputBlockSize * 2);

            printf("FloatResampler.setOutputSampleRate(): %d %d\n", _interp, _decim);

            float cutoff = std::min<float>(_outputSampleRate / 2.0f, _inputSampleRate / 2.0f);
            if (passBand > 0.0f && transWidth > 0.0f) {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            }
            else {
                dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, cutoff, cutoff);
            }
            
            start();
        }

        void setFilterParams(float passBand, float transWidth) {
            stop();
            dsp::BlackmanWindow(_taps, _inputSampleRate * _interp, passBand, transWidth);
            start();
        }

        void setBlockSize(int blockSize) {
            stop();
            _blockSize = blockSize;
            outputBlockSize = (_blockSize * _interp) / _decim;
            output.setMaxLatency(outputBlockSize * 2);
            start();
        }

        void setInput(stream<float>* input) {
            if (running) {
                return;
            }
            _input = input;
        }

        int getOutputBlockSize() {
            return outputBlockSize;
        }

        stream<float> output;

    private:
        static void _worker(FloatFIRResampler* _this) {
            float* inBuf = new float[_this->_blockSize];
            float* outBuf = new float[_this->outputBlockSize];
            
            int inCount = _this->_blockSize;
            int outCount = _this->outputBlockSize;
            
            float* taps = _this->_taps.data(); 
            int tapCount = _this->_taps.size();
            float* delayBuf = new float[tapCount];

            float* delayStart = &inBuf[std::max<int>(inCount - tapCount, 0)];
            int delaySize = tapCount * sizeof(float);
            float* delayBufEnd = &delayBuf[std::max<int>(tapCount - inCount, 0)];
            int moveSize = std::min<int>(inCount, tapCount - inCount) * sizeof(float);
            int inSize = inCount * sizeof(float);

            int interp = _this->_interp;
            int decim = _this->_decim;

            float correction = (float)sqrt((float)interp);

            printf("FloatResamp: %d %d", inCount, _this->outputBlockSize);
            
            int afterInterp = inCount * interp;
            int outIndex = 0;
            while (true) {
                if (_this->_input->read(inBuf, inCount) < 0) { break; };
                for (int i = 0; outIndex < outCount; i += decim) {
                    outBuf[outIndex] = 0;
                    for (int j = 0; j < tapCount; j++) {
                        if ((i - j) % interp != 0) {
                            continue;
                        }
                        outBuf[outIndex] += GET_FROM_RIGHT_BUF(inBuf, delayBuf, tapCount, (i - j) / interp) * taps[j] * correction;
                    }
                    outIndex++;
                }
                outIndex = 0;
                if (tapCount > inCount) {
                    memmove(delayBuf, delayBufEnd, moveSize);
                    memcpy(delayBufEnd, delayStart, inSize);
                }
                else {
                    memcpy(delayBuf, delayStart, delaySize);
                }
                
                if (_this->output.write(outBuf, _this->outputBlockSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
            delete[] delayBuf;
        }

        std::thread _workerThread;

        stream<float>* _input;
        std::vector<float> _taps;
        int _interp;
        int _decim;
        int outputBlockSize;
        float _outputSampleRate;
        float _inputSampleRate;
        int _blockSize;
        bool running = false;
    };
};