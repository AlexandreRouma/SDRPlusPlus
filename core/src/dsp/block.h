#pragma once
#include <vector>
#include <dsp/stream.h>
#include <volk.h>

namespace dsp {
    template <class D, class I, class O, int IC, int OC>
    class Block {
    public:
        Block(std::vector<int> inBs, std::vector<int> outBs, D* inst, void (*workerFunc)(D* _this)) {
            derived = inst;
            worker = workerFunc;
            inputBlockSize = inBs;
            outputBlockSize = outBs;
            in.reserve(IC);
            out.reserve(OC);
            for (int i = 0; i < IC; i++) {
                in.push_back(NULL);
            }
            for (int i = 0; i < OC; i++) {
                out.push_back(new stream<I>(outBs[i] * 2));
            }
        }

        void start() {
            if (running) {
                return;
            }
            running = true;
            startHandler();
            workerThread = std::thread(worker, derived);
        }

        void stop() {
            if (!running) {
                return;
            }
            stopHandler();
            for (auto is : in) {
                is->stopReader();
            }
            for (auto os : out) {
                os->stopWriter();
            }
            workerThread.join();
            
            for (auto is : in) {
                is->clearReadStop();
            }
            for (auto os : out) {
                os->clearWriteStop();
            }
            running = false;
        }

        virtual void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            for (int i = 0; i < IC; i++) {
                in[i]->setMaxLatency(blockSize * 2);
                inputBlockSize[i] = blockSize;
            }
            for (int i = 0; i < OC; i++) {
                out[i]->setMaxLatency(blockSize * 2);
                outputBlockSize[i] = blockSize;
            }
        }

        std::vector<stream<I>*> out;

    protected:
        virtual void startHandler() {}
        virtual void stopHandler() {}
        std::vector<stream<I>*> in;
        std::vector<int> inputBlockSize;
        std::vector<int> outputBlockSize;
        bool running = false;

    private:
        void (*worker)(D* _this);
        std::thread workerThread;
        D* derived;

    };


    class DemoMultiplier : public Block<DemoMultiplier, complex_t, complex_t, 2, 1> {
    public:
        DemoMultiplier() : Block({2}, {1}, this, worker) {}

        void init(stream<complex_t>* a, stream<complex_t>* b, int blockSize) {
            in[0] = a;
            in[1] = b;
            inputBlockSize[0] = blockSize;
            inputBlockSize[1] = blockSize;
            out[0]->setMaxLatency(blockSize * 2);
            outputBlockSize[0] = blockSize;
        }

    private:
        static void worker(DemoMultiplier* _this) {
            int blockSize = _this->inputBlockSize[0];
            stream<complex_t>* inA = _this->in[0];
            stream<complex_t>* inB = _this->in[1];
            stream<complex_t>* out = _this->out[0];
            complex_t* aBuf = (complex_t*)volk_malloc(sizeof(complex_t) * blockSize, volk_get_alignment());
            complex_t* bBuf = (complex_t*)volk_malloc(sizeof(complex_t) * blockSize, volk_get_alignment());
            complex_t* outBuf = (complex_t*)volk_malloc(sizeof(complex_t) * blockSize, volk_get_alignment());
            while (true) {
                if (inA->read(aBuf, blockSize) < 0) { break; };
                if (inB->read(bBuf, blockSize) < 0) { break; };
                volk_32fc_x2_multiply_32fc((lv_32fc_t*)outBuf, (lv_32fc_t*)aBuf, (lv_32fc_t*)bBuf, blockSize);
                if (out->write(outBuf, blockSize) < 0) { break; };
            }
            volk_free(aBuf);
            volk_free(bBuf);
            volk_free(outBuf);
        }

    };

    class Squelch : public Block<Squelch, complex_t, complex_t, 1, 1> {
    public:
        Squelch() : Block({1}, {1}, this, worker) {}

        void init(stream<complex_t>* input, int blockSize) {
            in[0] = input;
            inputBlockSize[0] = blockSize;
            out[0]->setMaxLatency(blockSize * 2);
            outputBlockSize[0] = blockSize;
            level = -50.0f;
        }

        float level;
        int onCount;
        int offCount;

    private:
        static void worker(Squelch* _this) {
            int blockSize = _this->inputBlockSize[0];
            stream<complex_t>* in = _this->in[0];
            stream<complex_t>* out = _this->out[0];
            complex_t* buf = new complex_t[blockSize];

            int _on = 0, _off = 0;
            bool active = false;

            while (true) {
                if (in->read(buf, blockSize) < 0) { break; };
                for (int i = 0; i < blockSize; i++) {
                    if (log10(sqrt((buf[i].i*buf[i].i) + (buf[i].q*buf[i].q))) * 10.0f > _this->level) {
                        _on++;
                        _off = 0;
                    }
                    else {
                        _on = 0;
                        _off++;
                    }
                    if (_on >= _this->onCount && !active) {
                        _on = _this->onCount;
                        active = true;
                    }
                    if (_off >= _this->offCount && active) {
                        _off = _this->offCount;
                        active = false;
                    }
                    if (!active) {
                        buf[i].i = 0.0f;
                        buf[i].q = 0.0f;
                    }
                }
                if (out->write(buf, blockSize) < 0) { break; };
            }
            delete[] buf;
        }

    };





    template <class T>
    class Reshaper {
    public:
        Reshaper() {

        }

        void init(int outBlockSize, dsp::stream<T>* input) {
            outputBlockSize = outBlockSize;
            in = input;
            out.init(outputBlockSize * 2);
        }

        void setOutputBlockSize(int blockSize) {
            if (running) {
                return;
            }
            outputBlockSize = blockSize;
            out.setMaxLatency(outputBlockSize * 2);
        }

        void setInput(dsp::stream<T>* input) {
            if (running) {
                return;
            }
            in = input;
        }

        void start() {
            if (running) {
                return;
            }
            workerThread = std::thread(_worker, this);
            running = true;
        }

        void stop() {
            if (!running) {
                return;
            }
            in->stopReader();
            out.stopWriter();
            workerThread.join();
            in->clearReadStop();
            out.clearWriteStop();
            running = false;
        }

        dsp::stream<T> out;

    private:
        static void _worker(Reshaper* _this) {
            T* buf = new T[_this->outputBlockSize];
            while (true) {
                if (_this->in->read(buf, _this->outputBlockSize) < 0) { break; }
                if (_this->out.write(buf, _this->outputBlockSize) < 0) { break; }
            }
            delete[] buf;
        }

        int outputBlockSize;
        bool running = false;
        std::thread workerThread;

        dsp::stream<T>* in;
        
    };
};