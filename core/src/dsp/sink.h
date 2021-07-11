#pragma once
#include <dsp/block.h>
#include <dsp/buffer.h>
#include <fstream>

namespace dsp {
    template <class T>
    class HandlerSink : public generic_block<HandlerSink<T>> {
    public:
        HandlerSink() {}

        HandlerSink(stream<T>* in, void (*handler)(T* data, int count, void* ctx), void* ctx) { init(in, handler, ctx); }

        void init(stream<T>* in, void (*handler)(T* data, int count, void* ctx), void* ctx) {
            _in = in;
            _handler = handler;
            _ctx = ctx;
            generic_block<HandlerSink<T>>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<HandlerSink<T>>::ctrlMtx);
            generic_block<HandlerSink<T>>::tempStop();
            generic_block<HandlerSink<T>>::unregisterInput(_in);
            _in = in;
            generic_block<HandlerSink<T>>::registerInput(_in);
            generic_block<HandlerSink<T>>::tempStart();
        }

        void setHandler(void (*handler)(T* data, int count, void* ctx), void* ctx) {
            std::lock_guard<std::mutex> lck(generic_block<HandlerSink<T>>::ctrlMtx);
            generic_block<HandlerSink<T>>::tempStop();
            _handler = handler;
            _ctx = ctx;
            generic_block<HandlerSink<T>>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            _handler(_in->readBuf, count, _ctx);
            _in->flush();
            return count;
        }

    private:
        stream<T>* _in;
        void (*_handler)(T* data, int count, void* ctx);
        void* _ctx;

    };

    template <class T>
    class RingBufferSink : public generic_block<RingBufferSink<T>> {
    public:
        RingBufferSink() {}

        RingBufferSink(stream<T>* in) { init(in); }

        void init(stream<T>* in) {
            _in = in;
            data.init(480); // TODO: Use an argument
            generic_block<RingBufferSink<T>>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<RingBufferSink<T>>::ctrlMtx);
            generic_block<RingBufferSink<T>>::tempStop();
            generic_block<RingBufferSink<T>>::unregisterInput(_in);
            _in = in;
            generic_block<RingBufferSink<T>>::registerInput(_in);
            generic_block<RingBufferSink<T>>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            if (data.write(_in->readBuf, count) < 0) { return -1; }
            _in->flush();
            return count;
        }

        RingBuffer<T> data;

    private:
        void doStop() {
            if (_in) _in->stopReader();
            data.stopWriter();
            if (generic_block<RingBufferSink<T>>::workerThread.joinable()) {
                generic_block<RingBufferSink<T>>::workerThread.join();
            }
            if (_in) _in->clearReadStop();
            data.clearWriteStop();
        }

        stream<T>* _in = NULL;

    };

    template <class T>
    class NullSink : public generic_block<NullSink<T>> {
    public:
        NullSink() {}

        NullSink(stream<T>* in) { init(in); }

        void init(stream<T>* in) {
            _in = in;
            generic_block<NullSink<T>>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<NullSink<T>>::ctrlMtx);
            generic_block<NullSink<T>>::tempStop();
            generic_block<NullSink<T>>::unregisterInput(_in);
            _in = in;
            generic_block<NullSink<T>>::registerInput(_in);
            generic_block<NullSink<T>>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            _in->flush();
            return count;
        }

    private:
        stream<T>* _in;

    };

    template <class T>
    class FileSink : public generic_block<FileSink<T>> {
    public:
        FileSink() {}

        FileSink(stream<T>* in, std::string path) { init(in, path); }

        ~FileSink() {
            generic_block<FileSink<T>>::stop();
            if (file.is_open()) { file.close(); }
        }

        void init(stream<T>* in, std::string path) {
            _in = in;
            file = std::ofstream(path, std::ios::binary);
            generic_block<FileSink<T>>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<FileSink<T>>::ctrlMtx);
            generic_block<FileSink<T>>::tempStop();
            generic_block<FileSink<T>>::unregisterInput(_in);
            _in = in;
            generic_block<FileSink<T>>::registerInput(_in);
            generic_block<FileSink<T>>::tempStart();
        }

        bool isOpen() {
            return file.is_open();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            if (file.is_open()) {
                file.write((char*)_in->readBuf, count * sizeof(T));
            }

            _in->flush();
            return count;
        }

    private:
        stream<T>* _in;
        std::ofstream file;

    };
}