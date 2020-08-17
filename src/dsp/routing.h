#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>
#include <spdlog/spdlog.h>

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

    template <class T>
    class DynamicSplitter {
    public:
        DynamicSplitter() {
            
        }

        DynamicSplitter(stream<T>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void init(stream<T>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
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
            int outputCount = outputs.size();
            for (int i = 0; i < outputCount; i++) {
                outputs[i]->stopWriter();
            }
            _workerThread.join();
            _in->clearReadStop();
            for (int i = 0; i < outputCount; i++) {
                outputs[i]->clearWriteStop();
            }
            running = false;
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _bufferSize = blockSize;
            int outputCount = outputs.size();
            for (int i = 0; i < outputCount; i++) {
                outputs[i]->setMaxLatency(blockSize * 2);
            }
        }

        void bind(stream<T>* stream) {
            if (running) {
                return;
            }
            outputs.push_back(stream);
        }

        void unbind(stream<T>* stream) {
            if (running) {
                return;
            }
            int outputCount = outputs.size();
            for (int i = 0; i < outputCount; i++) {
                if (outputs[i] == stream) {
                    outputs.erase(outputs.begin() + i);
                    return;
                }
            }
        }

    private:
        static void _worker(DynamicSplitter* _this) {
            T* buf = new T[_this->_bufferSize];
            int outputCount = _this->outputs.size();
            while (true) {
                if (_this->_in->read(buf, _this->_bufferSize) < 0) { break; };
                for (int i = 0; i < outputCount; i++) {
                    if (_this->outputs[i]->write(buf, _this->_bufferSize) < 0) { break; };
                }
            }
            delete[] buf;
        }

        stream<T>* _in;
        int _bufferSize;
        std::thread _workerThread;
        bool running = false;
        std::vector<stream<T>*> outputs;
    };


    class MonoToStereo {
    public:
        MonoToStereo() {
            
        }

        MonoToStereo(stream<float>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
            output.init(bufferSize * 2);
        }

        void init(stream<float>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
            output.init(bufferSize * 2);
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

        stream<StereoFloat_t> output;

    private:
        static void _worker(MonoToStereo* _this) {
            float* inBuf = new float[_this->_bufferSize];
            StereoFloat_t* outBuf = new StereoFloat_t[_this->_bufferSize];
            while (true) {
                if (_this->_in->read(inBuf, _this->_bufferSize) < 0) { break; };
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i].l = inBuf[i];
                    outBuf[i].r = inBuf[i];
                }
                if (_this->output.write(outBuf, _this->_bufferSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<float>* _in;
        int _bufferSize;
        std::thread _workerThread;
        bool running = false;
    };

    class StereoToMono {
    public:
        StereoToMono() {
            
        }

        StereoToMono(stream<StereoFloat_t>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void init(stream<StereoFloat_t>* input, int bufferSize) {
            _in = input;
            _bufferSize = bufferSize;
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

        stream<float> output;

    private:
        static void _worker(StereoToMono* _this) {
            StereoFloat_t* inBuf = new StereoFloat_t[_this->_bufferSize];
            float* outBuf = new float[_this->_bufferSize];
            while (true) {
                if (_this->_in->read(inBuf, _this->_bufferSize) < 0) { break; };
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i] = (inBuf[i].l + inBuf[i].r) / 2.0f;
                }
                if (_this->output.write(outBuf, _this->_bufferSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<StereoFloat_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
        bool running = false;
    };
};