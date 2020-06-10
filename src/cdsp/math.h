#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>

namespace cdsp {
    class Multiplier {
    public:
        Multiplier(stream<complex_t>* a, stream<complex_t>* b, int bufferSize) : output(bufferSize * 2) {
            _a = a;
            _b = b;
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output;

    private:
        static void _worker(Multiplier* _this) {
            complex_t* aBuf = new complex_t[_this->_bufferSize];
            complex_t* bBuf = new complex_t[_this->_bufferSize];
            complex_t* outBuf = new complex_t[_this->_bufferSize];
            while (true) {
                _this->_a->read(aBuf, _this->_bufferSize);
                _this->_b->read(bBuf, _this->_bufferSize);
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i].i = (aBuf[i].q * bBuf[i].i) + (bBuf[i].q * aBuf[i].i); // BC + AD
                    outBuf[i].q = (aBuf[i].q * bBuf[i].q) - (aBuf[i].i * bBuf[i].i);
                }
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        stream<complex_t>* _a;
        stream<complex_t>* _b;
        int _bufferSize;
        std::thread _workerThread;
    };
};