#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/source.h>
#include <dsp/math.h>

/*
    TODO:
        - Add a sample rate ajustment function to all demodulators
*/

#define FAST_ATAN2_COEF1 3.1415926535f / 4.0f
#define FAST_ATAN2_COEF2 3.0f * FAST_ATAN2_COEF1

inline float fast_arctan2(float y, float x) {
   float abs_y = fabs(y) + (1e-10);
   float r, angle;
   if (x>=0) {
      r = (x - abs_y) / (x + abs_y);
      angle = FAST_ATAN2_COEF1 - FAST_ATAN2_COEF1 * r;
   }
   else {
      r = (x + abs_y) / (abs_y - x);
      angle = FAST_ATAN2_COEF2 - FAST_ATAN2_COEF1 * r;
   }
   if (y < 0) {
       return -angle;
   }
   return angle;
}

namespace dsp {
    class FMDemodulator {
    public:
        FMDemodulator() {
            
        }

        FMDemodulator(stream<complex_t>* in, float deviation, long sampleRate, int blockSize) : output(blockSize * 2) {
            running = false;
            _input = in;
            _blockSize = blockSize;
            _phase = 0.0f;
            _deviation = deviation;
            _sampleRate = sampleRate;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / deviation);
        }

        void init(stream<complex_t>* in, float deviation, long sampleRate, int blockSize) {
            output.init(blockSize * 2);
            running = false;
            _input = in;
            _blockSize = blockSize;
            _phase = 0.0f;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / deviation);
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
            _input->stopReader();
            output.stopWriter();
            _workerThread.join();
            running = false;
            _input->clearReadStop();
            output.clearWriteStop();
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
            output.setMaxLatency(_blockSize * 2);
        }

        void setSampleRate(float sampleRate) {
            _sampleRate = sampleRate;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / _deviation);
        }

        void setDeviation(float deviation) {
            _deviation = deviation;
            _phasorSpeed = (2 * 3.1415926535) / (_sampleRate / _deviation);
        }

        stream<float> output;

    private:
        static void _worker(FMDemodulator* _this) {
            complex_t* inBuf = new complex_t[_this->_blockSize];
            float* outBuf = new float[_this->_blockSize];
            float diff = 0;
            float currentPhase = 0;
            while (true) {
                if (_this->_input->read(inBuf, _this->_blockSize) < 0) { return; };
                for (int i = 0; i < _this->_blockSize; i++) {
                    currentPhase = fast_arctan2(inBuf[i].i, inBuf[i].q);
                    diff = currentPhase - _this->_phase;
                    if (diff > 3.1415926535f)        { diff -= 2 * 3.1415926535f; }
                    else if (diff <= -3.1415926535f) { diff += 2 * 3.1415926535f; }
                    outBuf[i] = diff / _this->_phasorSpeed;
                    _this->_phase = currentPhase;
                }
                if (_this->output.write(outBuf, _this->_blockSize) < 0) { return; };
            }
        }

        stream<complex_t>* _input;
        bool running;
        int _blockSize;
        float _phase;
        float _phasorSpeed;
        float _deviation;
        float _sampleRate;
        std::thread _workerThread;
    };


    class AMDemodulator {
    public:
        AMDemodulator() {
            
        }

        AMDemodulator(stream<complex_t>* in, int blockSize) : output(blockSize * 2) {
            running = false;
            _input = in;
            _blockSize = blockSize;
        }

        void init(stream<complex_t>* in, int blockSize) {
            output.init(blockSize * 2);
            running = false;
            _input = in;
            _blockSize = blockSize;
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
            _input->stopReader();
            output.stopWriter();
            _workerThread.join();
            running = false;
            _input->clearReadStop();
            output.clearWriteStop();
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
            output.setMaxLatency(_blockSize * 2);
        }

        stream<float> output;

    private:
        static void _worker(AMDemodulator* _this) {
            complex_t* inBuf = new complex_t[_this->_blockSize];
            float* outBuf = new float[_this->_blockSize];
            float min, max, amp;
            while (true) {
                if (_this->_input->read(inBuf, _this->_blockSize) < 0) { break; };
                min = INFINITY;
                max = 0.0f;
                for (int i = 0; i < _this->_blockSize; i++) {
                    outBuf[i] = sqrt((inBuf[i].i*inBuf[i].i) + (inBuf[i].q*inBuf[i].q));
                    if (outBuf[i] < min) {
                        min = outBuf[i];
                    }
                    if (outBuf[i] > max) {
                        max = outBuf[i];
                    }
                }
                amp = (max - min) / 2.0f;
                for (int i = 0; i < _this->_blockSize; i++) {
                    outBuf[i] = (outBuf[i] - min - amp) / amp;
                }
                if (_this->output.write(outBuf, _this->_blockSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<complex_t>* _input;
        bool running;
        int _blockSize;
        std::thread _workerThread;
    };

    class SSBDemod {
    public:
        SSBDemod() {

        }

        void init(stream<complex_t>* input, float sampleRate, float bandWidth, int blockSize) {
            _blockSize = blockSize;
            _bandWidth = bandWidth;
            _mode = MODE_USB;
            output.init(blockSize * 2);
            lo.init(bandWidth / 2.0f, sampleRate, blockSize);
            mixer.init(input, &lo.output, blockSize);
            lo.start();
        }

        void start() {
            mixer.start();
            _workerThread = std::thread(_worker, this);
            running = true;
        }

        void stop() {
            mixer.stop();
            mixer.output.stopReader();
            output.stopWriter();
            _workerThread.join();
            mixer.output.clearReadStop();
            output.clearWriteStop();
            running = false;
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
        }

        void setMode(int mode) {
            if (mode < 0 && mode >= _MODE_COUNT) {
                return;
            }
            _mode = mode;
            if (mode == MODE_USB) {
                lo.setFrequency(_bandWidth / 2.0f);
            }
            else if (mode == MODE_LSB) {
                lo.setFrequency(-_bandWidth / 2.0f);
            }
            else if (mode == MODE_LSB) {
                lo.setFrequency(0);
            }
        }

        void setBandwidth(float bandwidth) {
            _bandWidth = bandwidth;
            if (_mode == MODE_USB) {
                lo.setFrequency(_bandWidth / 2.0f);
            }
            else if (_mode == MODE_LSB) {
                lo.setFrequency(-_bandWidth / 2.0f);
            }
        }

        stream<float> output;

        enum {
            MODE_USB,
            MODE_LSB,
            MODE_DSB,
            _MODE_COUNT
        };

    private:
        static void _worker(SSBDemod* _this) {
            complex_t* inBuf = new complex_t[_this->_blockSize];
            float* outBuf = new float[_this->_blockSize];

            float min, max, factor;

            while (true) {
                if (_this->mixer.output.read(inBuf, _this->_blockSize) < 0) { break; };
                min = INFINITY;
                max = -INFINITY;
                for (int i = 0; i < _this->_blockSize; i++) {
                    outBuf[i] = inBuf[i].q;
                    if (inBuf[i].q < min) {
                        min = inBuf[i].q;
                    }
                    if (inBuf[i].q > max) {
                        max = inBuf[i].q;
                    }
                }
                factor = (max - min) / 2;
                for (int i = 0; i < _this->_blockSize; i++) {
                    outBuf[i] /= factor;
                }
                if (_this->output.write(outBuf, _this->_blockSize) < 0) { break; };
            }

            delete[] inBuf;
            delete[] outBuf;
        }

        std::thread _workerThread;
        SineSource lo;
        Multiplier mixer;
        int _blockSize;
        float _bandWidth;
        int _mode;
        bool running = false;
    };





    // class CWDemod {
    // public:
    //     CWDemod() {

    //     }

    //     void init(stream<complex_t>* input, float sampleRate, float bandWidth, int blockSize) {
    //         _blockSize = blockSize;
    //         _bandWidth = bandWidth;
    //         _mode = MODE_USB;
    //         output.init(blockSize * 2);
    //         lo.init(bandWidth / 2.0f, sampleRate, blockSize);
    //         mixer.init(input, &lo.output, blockSize);
    //         lo.start();
    //     }

    //     void start() {
    //         mixer.start();
    //         _workerThread = std::thread(_worker, this);
    //         running = true;
    //     }

    //     void stop() {
    //         mixer.stop();
    //         mixer.output.stopReader();
    //         output.stopWriter();
    //         _workerThread.join();
    //         mixer.output.clearReadStop();
    //         output.clearWriteStop();
    //         running = false;
    //     }

    //     void setBlockSize(int blockSize) {
    //         if (running) {
    //             return;
    //         }
    //         _blockSize = blockSize;
    //     }

    //     void setMode(int mode) {
    //         if (mode < 0 && mode >= _MODE_COUNT) {
    //             return;
    //         }
    //         _mode = mode;
    //         if (mode == MODE_USB) {
    //             lo.setFrequency(_bandWidth / 2.0f);
    //         }
    //         else if (mode == MODE_LSB) {
    //             lo.setFrequency(-_bandWidth / 2.0f);
    //         }
    //     }

    //     stream<float> output;

    // private:
    //     static void _worker(CWDemod* _this) {
    //         complex_t* inBuf = new complex_t[_this->_blockSize];
    //         float* outBuf = new float[_this->_blockSize];

    //         float min, max, factor;

    //         while (true) {
    //             if (_this->mixer.output.read(inBuf, _this->_blockSize) < 0) { break; };
    //             min = INFINITY;
    //             max = -INFINITY;
    //             for (int i = 0; i < _this->_blockSize; i++) {
    //                 outBuf[i] = inBuf[i].q;
    //                 if (inBuf[i].q < min) {
    //                     min = inBuf[i].q;
    //                 }
    //                 if (inBuf[i].q > max) {
    //                     max = inBuf[i].q;
    //                 }
    //             }
    //             factor = (max - min) / 2;
    //             for (int i = 0; i < _this->_blockSize; i++) {
    //                 outBuf[i] /= factor;
    //             }
    //             if (_this->output.write(outBuf, _this->_blockSize) < 0) { break; };
    //         }

    //         delete[] inBuf;
    //         delete[] outBuf;
    //     }

    //     std::thread _workerThread;
    //     SineSource lo;
    //     Multiplier mixer;
    //     int _blockSize;
    //     float _bandWidth;
    //     int _mode;
    //     bool running = false;
    // };
};