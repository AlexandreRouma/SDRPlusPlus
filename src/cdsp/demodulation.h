#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>

#define FAST_ATAN2_COEF1 3.1415926535f / 4.0f
#define FAST_ATAN2_COEF2 3.0f * FAST_ATAN2_COEF1

inline float fast_arctan2(float y, float x)
{
   float abs_y = fabs(y)+1e-10;
   float r, angle;
   if (x>=0)
   {
      r = (x - abs_y) / (x + abs_y);
      angle = FAST_ATAN2_COEF1 - FAST_ATAN2_COEF1 * r;
   }
   else
   {
      r = (x + abs_y) / (abs_y - x);
      angle = FAST_ATAN2_COEF2 - FAST_ATAN2_COEF1 * r;
   }
   if (y < 0) {
       return -angle;
   }
   return angle;
}

namespace cdsp {
    class FMDemodulator {
    public:
        FMDemodulator() {
            
        }

        FMDemodulator(stream<complex_t>* in, float deviation, long sampleRate, int bufferSize) : output(bufferSize * 2) {
            running = false;
            _input = in;
            _bufferSize = bufferSize;
            _phase = 0.0f;
            _phasorSpeed = (2 * 3.1415926535) / (sampleRate / deviation);
        }

        void init(stream<complex_t>* in, float deviation, long sampleRate, int bufferSize) {
            output.init(bufferSize * 2);
            running = false;
            _input = in;
            _bufferSize = bufferSize;
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

        stream<float> output;

    private:
        static void _worker(FMDemodulator* _this) {
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            float* outBuf = new float[_this->_bufferSize];
            float diff = 0;
            float currentPhase = 0;
            while (true) {
                if (_this->_input->read(inBuf, _this->_bufferSize) < 0) { return; };
                for (int i = 0; i < _this->_bufferSize; i++) {
                    currentPhase = fast_arctan2(inBuf[i].i, inBuf[i].q);
                    diff = currentPhase - _this->_phase;
                    if (diff > 3.1415926535f)        { diff -= 2 * 3.1415926535f; }
                    else if (diff <= -3.1415926535f) { diff += 2 * 3.1415926535f; }
                    outBuf[i] = diff / _this->_phasorSpeed;
                    _this->_phase = currentPhase;
                }
                if (_this->output.write(outBuf, _this->_bufferSize) < 0) { return; };
            }
        }

        stream<complex_t>* _input;
        bool running;
        int _bufferSize;
        float _phase;
        float _phasorSpeed;
        std::thread _workerThread;
    };


    class AMDemodulator {
    public:
        AMDemodulator() {
            
        }

        AMDemodulator(stream<complex_t>* in, int bufferSize) : output(bufferSize * 2) {
            running = false;
            _input = in;
            _bufferSize = bufferSize;
        }

        void init(stream<complex_t>* in, int bufferSize) {
            output.init(bufferSize * 2);
            running = false;
            _input = in;
            _bufferSize = bufferSize;
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

        stream<float> output;

    private:
        static void _worker(AMDemodulator* _this) {
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            float* outBuf = new float[_this->_bufferSize];
            float min, max, amp;
            while (true) {
                if (_this->_input->read(inBuf, _this->_bufferSize) < 0) { return; };
                min = INFINITY;
                max = 0.0f;
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i] = sqrt((inBuf[i].i*inBuf[i].i) + (inBuf[i].q*inBuf[i].q));
                    if (outBuf[i] < min) {
                        min = outBuf[i];
                    }
                    if (outBuf[i] > max) {
                        max = outBuf[i];
                    }
                }
                amp = (max - min);
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i] = (outBuf[i] - min) / (max - min);
                }
                if (_this->output.write(outBuf, _this->_bufferSize) < 0) { return; };
            }
        }

        stream<complex_t>* _input;
        bool running;
        int _bufferSize;
        std::thread _workerThread;
    };
};