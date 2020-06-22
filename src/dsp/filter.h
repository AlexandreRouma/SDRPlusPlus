#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>
#include <dsp/math.h>

#define GET_FROM_RIGHT_BUF(buffer, delayLine, delayLineSz, n) (((n) < 0) ? delayLine[(delayLineSz) + (n)] : buffer[(n)])

namespace dsp {
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

    class DecimatingFIRFilter {
    public:
        DecimatingFIRFilter() {
            
        }

        DecimatingFIRFilter(stream<complex_t>* input, std::vector<float> taps, int blockSize, float decim) : output(blockSize * 2) {
            output.init(blockSize * 2);
            _in = input;
            _blockSize = blockSize;
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

        void init(stream<complex_t>* input, std::vector<float>& taps, int blockSize, float decim) {
            output.init(blockSize * 2);
            _in = input;
            _blockSize = blockSize;
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
            printf("[%d]\n", _tapCount);
            delete[] _taps;
            delete[] delayBuf;
            _taps = new float[_tapCount];
            delayBuf = new complex_t[_tapCount];
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
            output.setMaxLatency((_blockSize * 2) / _decim);
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
            output.setMaxLatency((_blockSize * 2) / _decim);
        }

        stream<complex_t> output;

    private:
        static void _worker(DecimatingFIRFilter* _this) {
            int outputSize = _this->_blockSize / _this->_decim;
            complex_t* inBuf = new complex_t[_this->_blockSize];
            complex_t* outBuf = new complex_t[outputSize];
            float tap = 0.0f;
            int delayOff;
            void* delayStart = &inBuf[_this->_blockSize - (_this->_tapCount - 1)];
            int delaySize = (_this->_tapCount - 1) * sizeof(complex_t);

            int blockSize = _this->_blockSize;
            int outBufferLength = outputSize * sizeof(complex_t);
            int tapCount = _this->_tapCount;
            int decim = _this->_decim;
            complex_t* delayBuf = _this->delayBuf;
            int id = 0;

            while (true) {
                if (_this->_in->read(inBuf, blockSize) < 0) { break; };
                memset(outBuf, 0, outBufferLength);
                
                for (int t = 0; t < tapCount; t++) {
                    tap = _this->_taps[t];
                    if (tap == 0.0f) {
                        continue;
                    }

                    delayOff = tapCount - t;
                    id = 0;

                    for (int i = 0; i < blockSize; i += decim) {
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
        int _blockSize;
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

        FloatDecimatingFIRFilter(stream<float>* input, std::vector<float> taps, int blockSize, float decim) : output(blockSize * 2) {
            output.init(blockSize * 2);
            _in = input;
            _blockSize = blockSize;
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

        void init(stream<float>* input, std::vector<float>& taps, int blockSize, float decim) {
            output.init(blockSize * 2);
            _in = input;
            _blockSize = blockSize;
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
            delete[] delayBuf;
            _taps = new float[_tapCount];
            delayBuf = new float[_tapCount];
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
            output.setMaxLatency((_blockSize * 2) / _decim);
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
            output.setMaxLatency((_blockSize * 2) / _decim);
        }

        stream<float> output;

    private:
        static void _worker(FloatDecimatingFIRFilter* _this) {
            int outputSize = _this->_blockSize / _this->_decim;
            float* inBuf = new float[_this->_blockSize];
            float* outBuf = new float[outputSize];
            float tap = 0.0f;
            int delayOff;
            void* delayStart = &inBuf[_this->_blockSize - (_this->_tapCount - 1)];
            int delaySize = (_this->_tapCount - 1) * sizeof(float);

            int blockSize = _this->_blockSize;
            int outBufferLength = outputSize * sizeof(complex_t);
            int tapCount = _this->_tapCount;
            int decim = _this->_decim;
            float* delayBuf = _this->delayBuf;
            int id = 0;

            while (true) {
                if (_this->_in->read(inBuf, blockSize) < 0) { break; };
                memset(outBuf, 0, outBufferLength);
                
                for (int t = 0; t < tapCount; t++) {
                    tap = _this->_taps[t];
                    if (tap == 0.0f) {
                        continue;
                    }

                    delayOff = tapCount - t;
                    id = 0;

                    for (int i = 0; i < blockSize; i += decim) {
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
        int _blockSize;
        int _tapCount = 0;
        float _decim;
        std::thread _workerThread;
        float* _taps;
        bool running;
    };
};