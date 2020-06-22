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
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output_a;
        stream<complex_t> output_b;

    private:
        static void _worker(Splitter* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            while (true) {
                _this->_in->read(buf, _this->_bufferSize);
                _this->output_a.write(buf, _this->_bufferSize);
                _this->output_b.write(buf, _this->_bufferSize);
            }
        }

        stream<complex_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
    };
};