#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>
#include <cmath>

namespace cdsp {
    class FMModulator {
    public:
        FMModulator(stream<float>* in, float deviation, long sampleRate, int bufferSize) : output(bufferSize * 2) {
            _input = in;
            _bufferSize = bufferSize;
            _phase = 0.0f;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / deviation);
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output;

    private:
        static void _worker(FMModulator* _this) {
            float* inBuf = new float[_this->_bufferSize];
            complex_t* outBuf = new complex_t[_this->_bufferSize];
            while (true) {
                _this->_input->read(inBuf, _this->_bufferSize);
                for (int i = 0; i < _this->_bufferSize; i++) {
                    _this->_phase += inBuf[i] * _this->_phasorSpeed;
                    outBuf[i].i = std::sinf(_this->_phase);
                    outBuf[i].q = std::cosf(_this->_phase);
                }
                _this->_phase = fmodf(_this->_phase, 2.0f * 3.1415926535);
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        stream<float>* _input;
        int _bufferSize;
        float _phase;
        float _phasorSpeed;
        std::thread _workerThread;
    };
};