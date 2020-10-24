#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>
#include <spdlog/spdlog.h>


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
            _workerThread.join();
            _in->clearReadStop();
            running = false;
        }

        bool bypass;

    private:
        static void _worker(HandlerSink* _this) {
            while (true) {
                if (_this->_in->read(_this->_buffer, _this->_bufferSize) < 0) { break; };
                _this->_handler(_this->_buffer);
            }
        }

        stream<complex_t>* _in;
        int _bufferSize;
        complex_t* _buffer;
        std::thread _workerThread;
        void (*_handler)(complex_t*);
        bool running = false;
    };

    template <class T>
    class NullSink {
    public:
        NullSink() {
            
        }

        NullSink(stream<T>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void init(stream<T>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        bool bypass;

    private:
        static void _worker(NullSink* _this) {
            T* buf = new T[_this->_bufferSize];
            while (true) {
                //spdlog::info("NS: Reading...");
                _this->_in->read(buf, _this->_bufferSize);
            }
        }

        stream<T>* _in;
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
            spdlog::info("NS: Starting...");
            _workerThread = std::thread(_worker, this);
        }

        bool bypass;

    private:
        static void _worker(FloatNullSink* _this) {
            spdlog::info("NS: Started!");
            float* buf = new float[_this->_bufferSize];
            while (true) {
                spdlog::info("NS: Reading...");
                _this->_in->read(buf, _this->_bufferSize);
            }
        }

        stream<float>* _in;
        int _bufferSize;
        std::thread _workerThread;
    };
};