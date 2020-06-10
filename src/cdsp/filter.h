#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>
#include <vector>

namespace cdsp {
    class FIRFilter {
    public:
        FIRFilter(stream<complex_t>* input, std::vector<float> taps, int bufferSize) : output(bufferSize * 2) {
            _in = input;
            _bufferSize = bufferSize;
            _tapCount = taps.size();
            delayBuf = new complex_t[_tapCount];
            _taps = taps;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output;

    private:
        static void _worker(FIRFilter* _this) {
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            complex_t* outBuf = new complex_t[_this->_bufferSize];
            float tap = 0.0f;
            while (true) {
                _this->_in->read(inBuf, _this->_bufferSize);
                for (int i = _this->_tapCount; i < _this->_bufferSize - _this->_tapCount; i++) {
                    outBuf[i].i = 0.0f;
                    outBuf[i].q = 0.0f;
                }
                for (int t = 0; t < _this->_tapCount; t++) {
                    tap = _this->_taps[t];
                    if (tap == 0.0f) {
                        continue;
                    }
                    for (int i = 0; i < t; i++) {
                        outBuf[i].i += tap * _this->delayBuf[_this->_tapCount - t - 1].i;
                        outBuf[i].q += tap * _this->delayBuf[_this->_tapCount - t - 1].q;
                    }
                    for (int i = t; i < _this->_bufferSize; i++) {
                        outBuf[i].i += tap * inBuf[i - t].i;
                        outBuf[i].q += tap * inBuf[i - t].q;
                    }
                }
                // for (int i = _this->_tapCount; i < _this->_bufferSize - _this->_tapCount; i++) {
                //     outBuf[i].i /= (float)_this->_tapCount;
                //     outBuf[i].q /= (float)_this->_tapCount;
                // }
                memcpy(_this->delayBuf, &inBuf[_this->_bufferSize - _this->_tapCount], _this->_tapCount * sizeof(complex_t));
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        stream<complex_t>* _in;
        complex_t* delayBuf;
        int _bufferSize;
        int _tapCount = 0;
        std::vector<float> _taps;
        std::thread _workerThread;
    };


    class DCBiasRemover {
    public:
        DCBiasRemover(stream<complex_t>* input, int bufferSize) : output(bufferSize * 2) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<complex_t> output;

    private:
        static void _worker(DCBiasRemover* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            float ibias = 0.0f;
            float qbias = 0.0f;
            while (true) {
                _this->_in->read(buf, _this->_bufferSize);
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
};