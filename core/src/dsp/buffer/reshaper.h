#pragma once
#include "../block.h"
#include "ring_buffer.h"

// IMPORTANT: THIS IS TRASH AND MUST BE REWRITTEN IN THE FUTURE

namespace dsp::buffer {
    // NOTE: I'm not proud of this, it's BAD and just taken from the previous DSP, but it works...
    template <class T>
    class Reshaper : public block {
        using base_type = block;
    public:
        Reshaper() {}

        Reshaper(stream<T>* in, int keep, int skip) { init(in, keep, skip); }

        // NOTE: For some reason, the base class destructor doesn't get called.... this is a temporary fix I guess
        // I also don't check for _block_init for the exact sample reason, something's weird
        ~Reshaper() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
        }

        void init(stream<T>* in, int keep, int skip) {
            _in = in;
            _keep = keep;
            _skip = skip;
            ringBuf.init(keep * 2);
            base_type::registerInput(_in);
            base_type::registerOutput(&out);
            base_type::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            base_type::unregisterInput(_in);
            _in = in;
            base_type::registerInput(_in);
            base_type::tempStart();
        }

        void setKeep(int keep) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _keep = keep;
            ringBuf.setMaxLatency(keep * 2);
            base_type::tempStart();
        }

        void setSkip(int skip) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _skip = skip;
            base_type::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            ringBuf.write(_in->readBuf, count);
            _in->flush();
            return count;
        }

        stream<T> out;

    private:
        void doStart() override {
            workThread = std::thread(&Reshaper<T>::loop, this);
            bufferWorkerThread = std::thread(&Reshaper<T>::bufferWorker, this);
        }

        void loop() {
            while (run() >= 0)
                ;
        }

        void doStop() override {
            _in->stopReader();
            ringBuf.stopReader();
            out.stopWriter();
            ringBuf.stopWriter();

            if (workThread.joinable()) {
                workThread.join();
            }
            if (bufferWorkerThread.joinable()) {
                bufferWorkerThread.join();
            }

            _in->clearReadStop();
            ringBuf.clearReadStop();
            out.clearWriteStop();
            ringBuf.clearWriteStop();
        }

        void bufferWorker() {
            T* buf = new T[_keep];
            bool delay = _skip < 0;

            int readCount = std::min<int>(_keep + _skip, _keep);
            int skip = std::max<int>(_skip, 0);
            int delaySize = (-_skip) * sizeof(T);
            int delayCount = (-_skip);

            T* start = &buf[std::max<int>(-_skip, 0)];
            T* delayStart = &buf[_keep + _skip];

            while (true) {
                if (delay) {
                    memmove(buf, delayStart, delaySize);
                    if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                        for (int i = 0; i < delayCount; i++) {
                            buf[i].re /= 10.0f;
                            buf[i].im /= 10.0f;
                        }
                    }
                }
                if (ringBuf.readAndSkip(start, readCount, skip) < 0) { break; };
                memcpy(out.writeBuf, buf, _keep * sizeof(T));
                if (!out.swap(_keep)) { break; }
            }
            delete[] buf;
        }

        stream<T>* _in;
        int _outBlockSize;
        RingBuffer<T> ringBuf;
        std::thread bufferWorkerThread;
        std::thread workThread;
        int _keep, _skip;
    };
}