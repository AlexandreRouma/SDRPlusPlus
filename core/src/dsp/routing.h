#pragma once
#include <dsp/block.h>
#include <dsp/buffer.h>
#include <string.h>
#include <numeric>
#include <spdlog/spdlog.h>

namespace dsp {
    template <class T>
    class Splitter : public generic_block<Splitter<T>> {
    public:
        Splitter() {}

        Splitter(stream<T>* in) { init(in); }

        void init(stream<T>* in) {
            _in = in;
            generic_block<Splitter>::registerInput(_in);
            generic_block<Splitter>::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(generic_block<Splitter>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            generic_block<Splitter>::unregisterInput(_in);
            _in = in;
            generic_block<Splitter>::registerInput(_in);
            generic_block<Splitter>::tempStart();
        }

        void bindStream(stream<T>* stream) {
            assert(generic_block<Splitter>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            out.push_back(stream);
            generic_block<Splitter>::registerOutput(stream);
            generic_block<Splitter>::tempStart();
        }

        void unbindStream(stream<T>* stream) {
            assert(generic_block<Splitter>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            generic_block<Splitter>::unregisterOutput(stream);
            out.erase(std::remove(out.begin(), out.end(), stream), out.end());
            generic_block<Splitter>::tempStart();
        }

    private:
        int run() {
            // TODO: If too slow, buffering might be necessary
            int count = _in->read();
            if (count < 0) { return -1; }
            for (const auto& stream : out) {
                memcpy(stream->writeBuf, _in->readBuf, count * sizeof(T));
                if (!stream->swap(count)) { return -1; }
            }
            _in->flush();
            return count;
        }

        stream<T>* _in;
        std::vector<stream<T>*> out;
    };

    template <class T>
    class StreamDoubler : public generic_block<StreamDoubler<T>> {
    public:
        StreamDoubler() {}

        StreamDoubler(stream<T>* in) { init(in); }

        void init(stream<T>* in) {
            _in = in;
            generic_block<StreamDoubler>::registerInput(_in);
            generic_block<StreamDoubler>::registerOutput(&outA);
            generic_block<StreamDoubler>::registerOutput(&outB);
            generic_block<StreamDoubler>::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(generic_block<StreamDoubler>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<StreamDoubler>::ctrlMtx);
            generic_block<StreamDoubler>::tempStop();
            generic_block<StreamDoubler>::unregisterInput(_in);
            _in = in;
            generic_block<StreamDoubler>::registerInput(_in);
            generic_block<StreamDoubler>::tempStart();
        }

        stream<T> outA;
        stream<T> outB;

    private:
        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            memcpy(outA.writeBuf, _in->readBuf, count * sizeof(T));
            memcpy(outB.writeBuf, _in->readBuf, count * sizeof(T));

            _in->flush();

            if (!outA.swap(count)) { return -1; }
            if (!outB.swap(count)) { return -1; }

            return count;
        }

        stream<T>* _in;
    };


    // NOTE: I'm not proud of this, it's BAD and just taken from the previous DSP, but it works...
    template <class T>
    class Reshaper : public generic_block<Reshaper<T>> {
    public:
        Reshaper() {}

        Reshaper(stream<T>* in, int keep, int skip) { init(in, keep, skip); }

        // NOTE: For some reason, the base class destructor doesn't get called.... this is a temporary fix I guess
        // I also don't check for _block_init for the exact sample reason, something's weird
        ~Reshaper() {
            if (!generic_block<Reshaper<T>>::_block_init) { return; }
            generic_block<Reshaper<T>>::stop();
        }

        void init(stream<T>* in, int keep, int skip) {
            _in = in;
            _keep = keep;
            _skip = skip;
            ringBuf.init(keep * 2);
            generic_block<Reshaper<T>>::registerInput(_in);
            generic_block<Reshaper<T>>::registerOutput(&out);
            generic_block<Reshaper<T>>::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(generic_block<Reshaper<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Reshaper<T>>::ctrlMtx);
            generic_block<Reshaper<T>>::tempStop();
            generic_block<Reshaper<T>>::unregisterInput(_in);
            _in = in;
            generic_block<Reshaper<T>>::registerInput(_in);
            generic_block<Reshaper<T>>::tempStart();
        }

        void setKeep(int keep) {
            assert(generic_block<Reshaper<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Reshaper<T>>::ctrlMtx);
            generic_block<Reshaper<T>>::tempStop();
            _keep = keep;
            ringBuf.setMaxLatency(keep * 2);
            generic_block<Reshaper<T>>::tempStart();
        }

        void setSkip(int skip) {
            assert(generic_block<Reshaper<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Reshaper<T>>::ctrlMtx);
            generic_block<Reshaper<T>>::tempStop();
            _skip = skip;
            generic_block<Reshaper<T>>::tempStart();
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