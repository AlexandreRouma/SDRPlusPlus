#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>
#include <cdsp/fast_math.h>

namespace cdsp {
    class Multiplier {
    public:
        Multiplier() {
            
        }

        Multiplier(stream<complex_t>* a, stream<complex_t>* b, int bufferSize) : output(bufferSize * 2) {
            _a = a;
            _b = b;
            _bufferSize = bufferSize;
        }

        void init(stream<complex_t>* a, stream<complex_t>* b, int bufferSize) {
            output.init(bufferSize * 2);
            _a = a;
            _b = b;
            _bufferSize = bufferSize;
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
            _a->stopReader();
            _b->stopReader();
            output.stopWriter();
            _workerThread.join();
            running = false;
            _a->clearReadStop();
            _b->clearReadStop();
            output.clearWriteStop();
        }

        stream<complex_t> output;

    private:
        static void _worker(Multiplier* _this) {
            complex_t* aBuf = new complex_t[_this->_bufferSize];
            complex_t* bBuf = new complex_t[_this->_bufferSize];
            complex_t* outBuf = new complex_t[_this->_bufferSize];
            while (true) {
                if (_this->_a->read(aBuf, _this->_bufferSize) < 0) { printf("A Received stop signal\n"); return; };
                if (_this->_b->read(bBuf, _this->_bufferSize) < 0) { printf("B Received stop signal\n"); return; };
                do_mul(aBuf, bBuf, _this->_bufferSize);
                if (_this->output.write(aBuf, _this->_bufferSize) < 0) { printf("OUT Received stop signal\n"); return; };
            }
        }

        stream<complex_t>* _a;
        stream<complex_t>* _b;
        int _bufferSize;
        bool running = false;
        std::thread _workerThread;
    };
};