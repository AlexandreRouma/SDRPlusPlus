#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>

namespace dsp {
    class Splitter {
    public:
        Splitter() {
            
        }

        Splitter(stream<complex_t>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
            output_a.init(bufferSize);
            output_b.init(bufferSize);
        }

        void init(stream<complex_t>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
            output_a.init(bufferSize);
            output_b.init(bufferSize);
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
            output_a.stopWriter();
            output_b.stopWriter();
            _workerThread.join();
            _in->clearReadStop();
            output_a.clearWriteStop();
            output_b.clearWriteStop();
            running = false;
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _bufferSize = blockSize;
            output_a.setMaxLatency(blockSize * 2);
            output_b.setMaxLatency(blockSize * 2);
        }

        stream<complex_t> output_a;
        stream<complex_t> output_b;

    private:
        static void _worker(Splitter* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            while (true) {
                if (_this->_in->read(buf, _this->_bufferSize) < 0) { break; };
                if (_this->output_a.write(buf, _this->_bufferSize) < 0) { break; };
                if (_this->output_b.write(buf, _this->_bufferSize) < 0) { break; };
            }
            delete[] buf;
        }

        stream<complex_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
        bool running = false;
    };
};