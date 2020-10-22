#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <volk.h>

#ifndef M_PI
#define M_PI    3.1415926535f
#endif

namespace dsp {
    class Multiplier {
    public:
        Multiplier() {
            
        }

        Multiplier(stream<complex_t>* a, stream<complex_t>* b, int blockSize) : output(blockSize * 2) {
            _a = a;
            _b = b;
            _blockSize = blockSize;
        }

        void init(stream<complex_t>* a, stream<complex_t>* b, int blockSize) {
            output.init(blockSize * 2);
            _a = a;
            _b = b;
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
            _a->stopReader();
            _b->stopReader();
            output.stopWriter();
            _workerThread.join();
            running = false;
            _a->clearReadStop();
            _b->clearReadStop();
            output.clearWriteStop();
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _blockSize = blockSize;
            output.setMaxLatency(blockSize * 2);
        }

        stream<complex_t> output;

    private:
        static void _worker(Multiplier* _this) {
            complex_t* aBuf = (complex_t*)volk_malloc(sizeof(complex_t) * _this->_blockSize, volk_get_alignment());
            complex_t* bBuf = (complex_t*)volk_malloc(sizeof(complex_t) * _this->_blockSize, volk_get_alignment());
            complex_t* outBuf = (complex_t*)volk_malloc(sizeof(complex_t) * _this->_blockSize, volk_get_alignment());
            while (true) {
                if (_this->_a->read(aBuf, _this->_blockSize) < 0) { break; };
                if (_this->_b->read(bBuf, _this->_blockSize) < 0) { break; };
                volk_32fc_x2_multiply_32fc((lv_32fc_t*)outBuf, (lv_32fc_t*)aBuf, (lv_32fc_t*)bBuf, _this->_blockSize);
                if (_this->output.write(outBuf, _this->_blockSize) < 0) { break; };
            }
            volk_free(aBuf);
            volk_free(bBuf);
            volk_free(outBuf);
        }

        stream<complex_t>* _a;
        stream<complex_t>* _b;
        int _blockSize;
        bool running = false;
        std::thread _workerThread;
    };
};