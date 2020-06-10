#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>

namespace cdsp {
    class SineSource {
    public:
        SineSource() {
            
        }

        SineSource(float frequency, long sampleRate, int bufferSize) : output(bufferSize * 2) {
            _bufferSize = bufferSize;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / frequency);
            _phase = 0;
        }

        void init(float frequency, long sampleRate, int bufferSize) {
            output.init(bufferSize * 2);
            _bufferSize = bufferSize;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / frequency);
            _phase = 0;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<float> output;

    private:
        static void _worker(SineSource* _this) {
            float* outBuf = new float[_this->_bufferSize];
            while (true) {
                for (int i = 0; i < _this->_bufferSize; i++) {
                    _this->_phase += _this->_phasorSpeed;
                    outBuf[i] = cos(_this->_phase);
                }
                _this->_phase = fmodf(_this->_phase, 2.0f * 3.1415926535);
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        int _bufferSize;
        float _phasorSpeed;
        float _phase;
        std::thread _workerThread;
    };

    class RandomSource {
    public:
        RandomSource() {
            
        }

        RandomSource(float frequency, long sampleRate, int bufferSize) : output(bufferSize * 2) {
            _bufferSize = bufferSize;
        }

        void init(float frequency, long sampleRate, int bufferSize) {
            output.init(bufferSize * 2);
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<float> output;

    private:
        static void _worker(RandomSource* _this) {
            float* outBuf = new float[_this->_bufferSize];
            while (true) {
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i] = ((float)rand() / ((float)RAND_MAX / 2.0f)) - 1.0f;
                }
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        int _bufferSize;
        std::thread _workerThread;
    };

    class ComplexSineSource {
    public:
        ComplexSineSource() {
            
        }

        ComplexSineSource(float frequency, long sampleRate, int bufferSize) : output(bufferSize * 2) {
            _bufferSize = bufferSize;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / frequency);
            _phase = 0;
        }

        void init(float frequency, long sampleRate, int bufferSize) {
            output.init(bufferSize * 2);
            _bufferSize = bufferSize;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / frequency);
            _phase = 0;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output;

    private:
        static void _worker(ComplexSineSource* _this) {
            complex_t* outBuf = new complex_t[_this->_bufferSize];
            while (true) {
                for (int i = 0; i < _this->_bufferSize; i++) {
                    _this->_phase += _this->_phasorSpeed;
                    outBuf[i].i = sin(_this->_phase);
                    outBuf[i].q = cos(_this->_phase);
                }
                _this->_phase = fmodf(_this->_phase, 2.0f * 3.1415926535);
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        int _bufferSize;
        float _phasorSpeed;
        float _phase;
        std::thread _workerThread;
    };
};