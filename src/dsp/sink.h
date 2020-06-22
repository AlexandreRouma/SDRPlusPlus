#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>

namespace dsp {
    class HandlerSink {
    public:
        HandlerSink() {
            
        }

        HandlerSink(stream<complex_t>* input, complex_t* buffer, int bufferSize, void handler(complex_t*)) {
            _in = input;
            _bufferSize = bufferSize;
            _buffer = buffer;
            _handler = handler;
        }

        void init(stream<complex_t>* input, complex_t* buffer, int bufferSize, void handler(complex_t*)) {
            _in = input;
            _bufferSize = bufferSize;
            _buffer = buffer;
            _handler = handler;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        bool bypass;

    private:
        static void _worker(HandlerSink* _this) {
            while (true) {
                _this->_in->read(_this->_buffer, _this->_bufferSize);
                _this->_handler(_this->_buffer);
            }
        }

        stream<complex_t>* _in;
        int _bufferSize;
        complex_t* _buffer;
        std::thread _workerThread;
        void (*_handler)(complex_t*);
    };

    class NullSink {
    public:
        NullSink() {
            
        }

        NullSink(stream<complex_t>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void init(stream<complex_t>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        bool bypass;

    private:
        static void _worker(NullSink* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            while (true) {
                _this->_in->read(buf, _this->_bufferSize);
            }
        }

        stream<complex_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
    };

    class FloatNullSink {
    public:
        FloatNullSink() {
            
        }

        FloatNullSink(stream<float>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void init(stream<float>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        bool bypass;

    private:
        static void _worker(FloatNullSink* _this) {
            float* buf = new float[_this->_bufferSize];
            while (true) {
                _this->_in->read(buf, _this->_bufferSize);
            }
        }

        stream<float>* _in;
        int _bufferSize;
        std::thread _workerThread;
    };
};