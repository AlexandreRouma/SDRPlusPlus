#pragma once
#include <thread>
#include <assert.h>
#include "../stream.h"
#include "../types.h"

namespace dsp::bench {
    template<class I,  class O>
    class SpeedTester {
    public:
        SpeedTester() {}

        SpeedTester(stream<I>* in, stream<O>* out) { init(in, out); }

        void init(stream<I>* in, stream<O>* out) {
            _in = in;
            _out = out;
            _init = true;
        }

        void setInput(stream<I>* in) {
            assert(_init);
            _in = in;
        }

        void setOutput(stream<O>* out) {
            assert(_init);
            _out = out;
        }

        double benchmark(int durationMs, int bufferSize) {
            assert(_init);

            // Allocate and fill buffer
            inCount = bufferSize;
            randBuf = buffer::alloc<I>(inCount);
            for (int i = 0; i < inCount; i++) {
                if constexpr (std::is_same_v<I, complex_t>) {
                    randBuf[i].re = (2.0f * (float)rand() / (float)RAND_MAX) - 1.0f;
                    randBuf[i].im = (2.0f * (float)rand() / (float)RAND_MAX) - 1.0f;
                }
                else if constexpr (std::is_same_v<I, float>) {
                    randBuf[i] = (2.0f * (float)rand() / (float)RAND_MAX) - 1.0f;
                }
                else {
                    randBuf[i] = rand();
                }
            }

            // Run test
            start();
            std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
            stop();
            buffer::free(randBuf);
            return (double)sampCount * 1000.0 / (double)durationMs;
        }

    protected:
        void start() {
            if (running) { return; }
            running = true;
            sampCount = 0;
            wthr = std::thread(&SpeedTester::writeWorker, this);
            rthr = std::thread(&SpeedTester::readWorker, this);
        }

        void stop() {
            if (!running) { return; }
            running = false;
            _in->stopWriter();
            _out->stopReader();
            if (wthr.joinable()) { wthr.join(); }
            if (rthr.joinable()) { rthr.join(); }
            _in->clearWriteStop();
            _out->clearReadStop();
        }

        void writeWorker() {
            while (true) {
                memcpy(_in->writeBuf, randBuf, inCount * sizeof(I));
                if (!_in->swap(inCount)) { return; }
                sampCount += inCount;
            }
        }

        void readWorker() {
            while (true) {
                int count = _out->read();
                _out->flush();
                if (count < 0) { return; }
            }
        }

        bool _init = false;
        bool running = false;
        int inCount;
        stream<I>* _in;
        stream<O>* _out;
        I* randBuf;
        std::thread wthr;
        std::thread rthr;
        uint64_t sampCount;

    };
}