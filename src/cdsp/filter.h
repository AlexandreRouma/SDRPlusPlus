#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>
#include <vector>

#define GET_FROM_RIGHT_BUF(buffer, delayLine, delayLineSz, n) (((n) < 0) ? delayLine[(delayLineSz) + (n)] : buffer[(n)])
#define M_PI    3.1415926535f


inline void BlackmanWindow(std::vector<float>& taps, float sampleRate, float cutoff, float transWidth) {
    taps.clear();
    float fc = cutoff / sampleRate;
    int _M = 4.0f / (transWidth / sampleRate);
    if (_M % 2 == 0) { _M++; }
    float M = _M;
    float sum = 0.0f;
    for (int i = 0; i < _M; i++) {
        float val = (sin(2.0f * M_PI * fc * ((float)i - (M / 2))) / ((float)i - (M / 2))) * (0.42f - (0.5f * cos(2.0f * M_PI / M)) + (0.8f * cos(4.0f * M_PI / M)));
        taps.push_back(val);
        sum += val;
    }
    for (int i = 0; i < M; i++) {
        taps[i] /= sum;
    }
}

namespace cdsp {
    class DecimatingFIRFilter {
    public:
        DecimatingFIRFilter() {
            
        }

        DecimatingFIRFilter(stream<complex_t>* input, std::vector<float> taps, int bufferSize, float decim) : output(bufferSize * 2) {
            output.init(bufferSize * 2);
            _in = input;
            _bufferSize = bufferSize;
            _tapCount = taps.size();
            delayBuf = new complex_t[_tapCount];

            _taps = new float[_tapCount];
            for (int i = 0; i < _tapCount; i++) {
                _taps[i] = taps[i];
            }

            _decim = decim;

            for (int i = 0; i < _tapCount; i++) {
                delayBuf[i].i = 0.0f;
                delayBuf[i].q = 0.0f;
            }

            running = false;
        }

        void init(stream<complex_t>* input, std::vector<float>& taps, int bufferSize, float decim) {
            output.init(bufferSize * 2);
            _in = input;
            _bufferSize = bufferSize;
            _tapCount = taps.size();
            delayBuf = new complex_t[_tapCount];

            _taps = new float[_tapCount];
            for (int i = 0; i < _tapCount; i++) {
                _taps[i] = taps[i];
            }

            _decim = decim;

            for (int i = 0; i < _tapCount; i++) {
                delayBuf[i].i = 0.0f;
                delayBuf[i].q = 0.0f;
            }

            running = false;
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
            _in->stopReader();
            output.stopWriter();
            _workerThread.join();
            _in->clearReadStop();
            output.clearWriteStop();
            running = false;
        }

        void setTaps(std::vector<float>& taps) {
            if (running) {
                return;
            }
            _tapCount = taps.size();
            delete[] _taps;
            _taps = new float[_tapCount];
            for (int i = 0; i < _tapCount; i++) {
                _taps[i] = taps[i];
            }
        }

        void setInput(stream<complex_t>* input) {
            if (running) {
                return;
            }
            _in = input;
        }

        void setDecimation(float decimation) {
            if (running) {
                return;
            }
            _decim = decimation;
        }

        void setBufferSize(int bufferSize) {
            if (running) {
                return;
            }
            _bufferSize = bufferSize;
        }

        stream<complex_t> output;

    private:
        static void _worker(DecimatingFIRFilter* _this) {
            int outputSize = _this->_bufferSize / _this->_decim;
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            complex_t* outBuf = new complex_t[outputSize];
            float tap = 0.0f;
            int delayOff;
            void* delayStart = &inBuf[_this->_bufferSize - (_this->_tapCount - 1)];
            int delaySize = (_this->_tapCount - 1) * sizeof(complex_t);

            int bufferSize = _this->_bufferSize;
            int outBufferLength = outputSize * sizeof(complex_t);
            int tapCount = _this->_tapCount;
            int decim = _this->_decim;
            complex_t* delayBuf = _this->delayBuf;
            int id = 0;

            while (true) {
                if (_this->_in->read(inBuf, bufferSize) < 0) { break; };
                memset(outBuf, 0, outBufferLength);
                
                for (int t = 0; t < tapCount; t++) {
                    tap = _this->_taps[t];
                    if (tap == 0.0f) {
                        continue;
                    }

                    delayOff = tapCount - t;
                    id = 0;

                    for (int i = 0; i < bufferSize; i += decim) {
                        if (i < t) {
                            outBuf[id].i += tap * delayBuf[delayOff + i].i;
                            outBuf[id].q += tap * delayBuf[delayOff + i].q;
                            id++;
                            continue;
                        }
                        outBuf[id].i += tap * inBuf[i - t].i;
                        outBuf[id].q += tap * inBuf[i - t].q;
                        id++;
                    }
                }
                memcpy(delayBuf, delayStart, delaySize);
                if (_this->output.write(outBuf, outputSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<complex_t>* _in;
        complex_t* delayBuf;
        int _bufferSize;
        int _tapCount = 0;
        float _decim;
        std::thread _workerThread;
        float* _taps;
        bool running;
    };

    class FloatDecimatingFIRFilter {
    public:
        FloatDecimatingFIRFilter() {
            
        }

        FloatDecimatingFIRFilter(stream<float>* input, std::vector<float> taps, int bufferSize, float decim) : output(bufferSize * 2) {
            output.init(bufferSize * 2);
            _in = input;
            _bufferSize = bufferSize;
            _tapCount = taps.size();
            delayBuf = new float[_tapCount];

            _taps = new float[_tapCount];
            for (int i = 0; i < _tapCount; i++) {
                _taps[i] = taps[i];
            }

            _decim = decim;

            for (int i = 0; i < _tapCount; i++) {
                delayBuf[i] = 0.0f;
            }

            running = false;
        }

        void init(stream<float>* input, std::vector<float>& taps, int bufferSize, float decim) {
            output.init(bufferSize * 2);
            _in = input;
            _bufferSize = bufferSize;
            _tapCount = taps.size();
            delayBuf = new float[_tapCount];

            _taps = new float[_tapCount];
            for (int i = 0; i < _tapCount; i++) {
                _taps[i] = taps[i];
            }

            _decim = decim;

            for (int i = 0; i < _tapCount; i++) {
                delayBuf[i] = 0.0f;
            }

            running = false;
        }

        void start() {
            running = true;
            _workerThread = std::thread(_worker, this);
        }
        
        void stop() {
            _in->stopReader();
            output.stopWriter();
            _workerThread.join();
            _in->clearReadStop();
            output.clearWriteStop();
            running = false;
        }

        void setTaps(std::vector<float>& taps) {
            if (running) {
                return;
            }
            _tapCount = taps.size();
            delete[] _taps;
            _taps = new float[_tapCount];
            for (int i = 0; i < _tapCount; i++) {
                _taps[i] = taps[i];
            }
        }

        void setInput(stream<float>* input) {
            if (running) {
                return;
            }
            _in = input;
        }

        void setDecimation(float decimation) {
            if (running) {
                return;
            }
            _decim = decimation;
        }

        stream<float> output;

    private:
        static void _worker(FloatDecimatingFIRFilter* _this) {
            int outputSize = _this->_bufferSize / _this->_decim;
            float* inBuf = new float[_this->_bufferSize];
            float* outBuf = new float[outputSize];
            float tap = 0.0f;
            int delayOff;
            void* delayStart = &inBuf[_this->_bufferSize - (_this->_tapCount - 1)];
            int delaySize = (_this->_tapCount - 1) * sizeof(float);

            int bufferSize = _this->_bufferSize;
            int outBufferLength = outputSize * sizeof(float);
            int tapCount = _this->_tapCount;
            int decim = _this->_decim;
            float* delayBuf = _this->delayBuf;
            int id = 0;

            while (true) {
                if (_this->_in->read(inBuf, bufferSize) < 0) { break; };
                memset(outBuf, 0, outBufferLength);
                
                for (int t = 0; t < tapCount; t++) {
                    tap = _this->_taps[t];
                    if (tap == 0.0f) {
                        continue;
                    }

                    delayOff = tapCount - t;
                    id = 0;

                    for (int i = 0; i < bufferSize; i += decim) {
                        if (i < t) {
                            outBuf[id] += tap * delayBuf[delayOff + i];
                            id++;
                            continue;
                        }
                        outBuf[id] += tap * inBuf[i - t];
                        id++;
                    }
                }
                memcpy(delayBuf, delayStart, delaySize);
                if (_this->output.write(outBuf, outputSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<float>* _in;
        float* delayBuf;
        int _bufferSize;
        int _tapCount = 0;
        float _decim;
        std::thread _workerThread;
        float* _taps;
        bool running;
    };

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
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output;
        bool bypass;

    private:
        static void _worker(DCBiasRemover* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            float ibias = 0.0f;
            float qbias = 0.0f;
            while (true) {
                _this->_in->read(buf, _this->_bufferSize);
                if (_this->bypass) {
                    _this->output.write(buf, _this->_bufferSize);
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
                _this->output.write(buf, _this->_bufferSize);
            }
        }

        stream<complex_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
    };

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