#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>


namespace cdsp {
    class FMDemodulator {
    public:
        FMDemodulator(stream<complex_t>* in, float deviation, long sampleRate, int bufferSize) : output(bufferSize * 2) {
            _input = in;
            _bufferSize = bufferSize;
            _phase = 0.0f;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / deviation);
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<float> output;

    private:
        static void _worker(FMDemodulator* _this) {
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            float* outBuf = new float[_this->_bufferSize];
            float diff = 0;
            float currentPhase = 0;
            while (true) {
                _this->_input->read(inBuf, _this->_bufferSize);
                for (int i = 0; i < _this->_bufferSize; i++) {
                    currentPhase = atan2f(inBuf[i].i, inBuf[i].q);
                    diff = currentPhase - _this->_phase;
                    outBuf[i] = diff / _this->_phasorSpeed;
                    _this->_phase = currentPhase;
                }
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        stream<complex_t>* _input;
        int _bufferSize;
        float _phase;
        float _phasorSpeed;
        std::thread _workerThread;
    };
};