#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>

namespace dsp {
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

        void setInput(stream<complex_t>* input) {
            if (running) {
                return;
            }
            _in = input;
        }

        stream<complex_t> output;
        bool bypass;

    private:
        // static void _worker(DCBiasRemover* _this) {
        //     complex_t* buf = new complex_t[_this->_bufferSize];
        //     float ibias = 0.0f;
        //     float qbias = 0.0f;
        //     while (true) {
        //         if (_this->_in->read(buf, _this->_bufferSize) < 0) { break; };
        //         if (_this->bypass) {
        //             if (_this->output.write(buf, _this->_bufferSize) < 0) { break; };
        //             continue;
        //         }
        //         for (int i = 0; i < _this->_bufferSize; i++) {
        //             ibias += buf[i].i;
        //             qbias += buf[i].q;
        //         }
        //         ibias /= _this->_bufferSize;
        //         qbias /= _this->_bufferSize;
        //         for (int i = 0; i < _this->_bufferSize; i++) {
        //             buf[i].i -= ibias;
        //             buf[i].q -= qbias;
        //         }
        //         if (_this->output.write(buf, _this->_bufferSize) < 0) { break; };
        //     }
        //     delete[] buf;
        // }

        static void _worker(DCBiasRemover* _this) {
            complex_t* buf = new complex_t[_this->_bufferSize];
            complex_t* mixBuf = new complex_t[_this->_bufferSize];

            float currentPhase = 0.0f;
            float lastPhase = 0.0f;
            double phase = 0.0f;
            
            while (true) {
                float ibias = 0.0f;
                float qbias = 0.0f;
                if (_this->_in->read(buf, _this->_bufferSize) < 0) { break; };
                if (_this->bypass) {
                    if (_this->output.write(buf, _this->_bufferSize) < 0) { break; };
                    continue;
                }

                // Detect the frequency of the signal
                double avgDiff = 0.0f;
                // for (int i = 0; i < _this->_bufferSize; i++) {
                //     currentPhase = fast_arctan2(buf[i].i, buf[i].q);
                //     float diff = currentPhase - lastPhase;
                //     if (diff > 3.1415926535f)        { diff -= 2 * 3.1415926535f; }
                //     else if (diff <= -3.1415926535f) { diff += 2 * 3.1415926535f; }
                //     avgDiff += diff;
                //     lastPhase = currentPhase;
                // }
                // avgDiff /= (double)_this->_bufferSize;
                // avgDiff /= (double)_this->_bufferSize;

                // Average the samples to "filter" the signal to the block frequency
                for (int i = 0; i < _this->_bufferSize; i++) {
                    ibias += buf[i].i;
                    qbias += buf[i].q;
                }
                ibias /= _this->_bufferSize;
                qbias /= _this->_bufferSize;

                // Get the phase difference from the last block
                currentPhase = fast_arctan2(ibias, qbias);
                float diff = currentPhase - lastPhase;
                if (diff > 3.1415926535f)        { diff -= 2 * 3.1415926535f; }
                else if (diff <= -3.1415926535f) { diff += 2 * 3.1415926535f; }
                avgDiff += diff;
                lastPhase = currentPhase;
                avgDiff /= (double)_this->_bufferSize;

                // Generate a correction signal using the phase difference
                for (int i = 0; i < _this->_bufferSize; i++) {
                    mixBuf[i].i = sin(phase);
                    mixBuf[i].q = cos(phase);
                    phase -= avgDiff;
                    phase = fmodl(phase, 2.0 * 3.1415926535);
                }

                // Mix the correction signal with the original signal to shift the unwanted signal
                // to the center. Also, null out the real component so that symetric
                //  frequencies are removed (at least I hope...)
                float tq;
                for (int i = 0; i < _this->_bufferSize; i++) {
                    buf[i].i = ((mixBuf[i].i * buf[i].q) + (mixBuf[i].q * buf[i].i)) * 1.4142;
                    buf[i].q = 0;
                }

                if (_this->output.write(buf, _this->_bufferSize) < 0) { break; };
            }
            delete[] buf;
        }

        stream<complex_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
        bool running = false;
    };

    class ComplexToStereo {
    public:
        ComplexToStereo() {
            
        }

        ComplexToStereo(stream<complex_t>* input, int bufferSize) : output(bufferSize * 2) {
            _in = input;
            _bufferSize = bufferSize;
        }

        void init(stream<complex_t>* input, int bufferSize) {
            output.init(bufferSize * 2);
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

        stream<StereoFloat_t> output;

    private:
        static void _worker(ComplexToStereo* _this) {
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            StereoFloat_t* outBuf = new StereoFloat_t[_this->_bufferSize];
            while (true) {
                if (_this->_in->read(inBuf, _this->_bufferSize) < 0) { break; };
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i].l = inBuf[i].i;
                    outBuf[i].r = inBuf[i].q;
                }
                if (_this->output.write(outBuf, _this->_bufferSize) < 0) { break; };
            }
            delete[] inBuf;
            delete[] outBuf;
        }

        stream<complex_t>* _in;
        int _bufferSize;
        std::thread _workerThread;
        bool running = false;
    };
};