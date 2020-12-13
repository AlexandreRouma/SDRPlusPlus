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

        ~Splitter() { generic_block<Splitter>::stop(); }

        void init(stream<T>* in) {
            _in = in;
            generic_block<Splitter>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            generic_block<Splitter>::unregisterInput(_in);
            _in = in;
            generic_block<Splitter>::registerInput(_in);
            generic_block<Splitter>::tempStart();
        }

        void bindStream(stream<T>* stream) {
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            out.push_back(stream);
            generic_block<Splitter>::registerOutput(stream);
            generic_block<Splitter>::tempStart();
        }

        void unbindStream(stream<T>* stream) {
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
                if (stream->aquire() < 0) { return -1; } 
                memcpy(stream->data, _in->data, count * sizeof(T));
                stream->write(count);
            }
            _in->flush();
            return count;
        }

        stream<T>* _in;
        std::vector<stream<T>*> out;

    };


    // NOTE: I'm not proud of this, it's BAD and just taken from the previous DSP, but it works...
    template <class T>
    class Reshaper : public generic_block<Reshaper<T>> {
    public:
        Reshaper() {}

        Reshaper(stream<T>* in, int keep, int skip) { init(in, keep, skip); }

        ~Reshaper() { generic_block<Reshaper<T>>::stop(); }

        void init(stream<T>* in, int keep, int skip) {
            _in = in;
            _keep = keep;
            _skip = skip;
            ringBuf.init(keep * 2);
            generic_block<Reshaper<T>>::registerInput(_in);
            generic_block<Reshaper<T>>::registerOutput(&out);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<Reshaper<T>>::ctrlMtx);
            generic_block<Reshaper<T>>::tempStop();
            generic_block<Reshaper<T>>::unregisterInput(_in);
            _in = in;
            generic_block<Reshaper<T>>::registerInput(_in);
            generic_block<Reshaper<T>>::tempStart();
        }

        void setKeep(int keep) {
            std::lock_guard<std::mutex> lck(generic_block<Reshaper<T>>::ctrlMtx);
            generic_block<Reshaper<T>>::tempStop();
            generic_block<Reshaper<T>>::unregisterInput(_in);
            _keep = keep;
            ringBuf.setMaxLatency(keep * 2);
            generic_block<Reshaper<T>>::registerInput(_in);
            generic_block<Reshaper<T>>::tempStart();
        }

        void setSkip(int skip) {
            std::lock_guard<std::mutex> lck(generic_block<Reshaper<T>>::ctrlMtx);
            generic_block<Reshaper<T>>::tempStop();
            generic_block<Reshaper<T>>::unregisterInput(_in);
            _skip = skip;
            generic_block<Reshaper<T>>::registerInput(_in);
            generic_block<Reshaper<T>>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            ringBuf.write(_in->data, count);
            _in->flush();
            return count;
        }

        stream<T> out;

    private:
        void doStart() {
            workThread = std::thread(&Reshaper<T>::loop, this);
            bufferWorkerThread = std::thread(&Reshaper<T>::bufferWorker, this);
        }

        void loop() {
            while (run() >= 0);
        }

        void doStop() {
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
            complex_t* buf = new complex_t[_keep];
            bool delay = _skip < 0;

            int readCount = std::min<int>(_keep + _skip, _keep);
            int skip = std::max<int>(_skip, 0);
            int delaySize = (-_skip) * sizeof(complex_t);

            complex_t* start = &buf[std::max<int>(-_skip, 0)];
            complex_t* delayStart = &buf[_keep + _skip];

            while (true) {
                if (delay) {
                    memmove(buf, delayStart, delaySize);
                }
                if (ringBuf.readAndSkip(start, readCount, skip) < 0) { break; };
                if (out.aquire() < 0) { break; }
                memcpy(out.data, buf, _keep * sizeof(complex_t));
                out.write(_keep);
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