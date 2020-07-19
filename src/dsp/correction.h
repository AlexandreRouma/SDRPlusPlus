#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>

namespace dsp {
    class DCBiasRemover {
    public:
        DCBiasRemover() {
            
        }

        DCBiasRemover(stream<complex_t>* input, int bufferSize) : output(bufferSize * 2) {
            _in = input;
            _bufferSize = bufferSize;
            bypass = false;
        }

        void init(stream<complex_t>* input, int bufferSize) {
            output.init(bufferSize * 2);
            _in = input;
            _bufferSize = bufferSize;
            bypass = false;
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
            _in->stopReader();
            output.stopWriter();
            _workerThread.join();
            _in->clearReadStop();
            output.clearWriteStop();
            running = false;
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _bufferSize = blockSize;
            output.setMaxLatency(blockSize * 2);
        }

        stream<complex_t> output;
        bool bypass;

    private:
        static void _worker(DCBiasRemover* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            float ibias = 0.0f;
            float qbias = 0.0f;
            while (true) {
                if (_this->_in->read(buf, _this->_bufferSize) < 0) { break; };
                if (_this->bypass) {
                    if (_this->output.write(buf, _this->_bufferSize) < 0) { break; };
                    continue;
                }
                for (int i = 0; i < _this->_bufferSize; i++) {
                    ibias += buf[i].i;
                    qbias += buf[i].q;
                }
                ibias /= _this->_bufferSize;
                qbias /= _this->_bufferSize;
                for (int i = 0; i < _this->_bufferSize; i++) {
                    buf[i].i -= ibias;
                    buf[i].q -= qbias;
                }
                if (_this->output.write(buf, _this->_bufferSize) < 0) { break; };
            }
            delete[] buf;
        }

        stream<complex_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
        bool running = false;
    };
};