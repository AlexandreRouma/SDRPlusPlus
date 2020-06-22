#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <volk.h>

namespace dsp {
    class SineSource {
    public:
        SineSource() {
            
        }

        SineSource(float frequency, long sampleRate, int blockSize) : output(blockSize * 2) {
            _blockSize = blockSize;
            _sampleRate = sampleRate;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / frequency);
            _phase = 0;
        }

        void init(float frequency, long sampleRate, int blockSize) {
            output.init(blockSize * 2);
            _sampleRate = sampleRate;
            _blockSize = blockSize;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / frequency);
            _phase = 0;
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
            output.stopWriter();
            _workerThread.join();
            output.clearWriteStop();
            running = false;
        }

        void setFrequency(float frequency) {
            _phasorSpeed = (2 * 3.1415926535) / (_sampleRate / frequency);
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
        }

        stream<complex_t> output;

    private:
        static void _worker(SineSource* _this) {
            complex_t* outBuf = new complex_t[_this->_blockSize];
            while (true) {
                for (int i = 0; i < _this->_blockSize; i++) {
                    _this->_phase += _this->_phasorSpeed;
                    outBuf[i].i = sin(_this->_phase);
                    outBuf[i].q = cos(_this->_phase);
                }
                _this->_phase = fmodf(_this->_phase, 2.0f * 3.1415926535);
                if (_this->output.write(outBuf, _this->_blockSize) < 0) { break; };
            }
            delete[] outBuf;
        }

        int _blockSize;
        float _phasorSpeed;
        float _phase;
        long _sampleRate;
        std::thread _workerThread;
        bool running = false;
    };
};