#pragma once
#include <dsp/block.h>
#include <dsp/buffer.h>

namespace dsp {
    template <class T>
    class HandlerSink : public generic_block<HandlerSink<T>> {
    public:
        HandlerSink() {}

        HandlerSink(stream<T>* in, void (*handler)(T* data, int count, void* ctx), void* ctx) { init(in, handler, ctx); }

        ~HandlerSink() { generic_block<HandlerSink<T>>::stop(); }

        void init(stream<T>* in, void (*handler)(T* data, int count, void* ctx), void* ctx) {
            _in = in;
            _handler = handler;
            _ctx = ctx;
            generic_block<HandlerSink<T>>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<HandlerSink<T>>::ctrlMtx);
            generic_block<HandlerSink<T>>::tempStop();
            _in = in;
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
            count = _in->read();
            if (count < 0) { return -1; }
            _handler(_in->data, count, _ctx);
            _in->flush();
            return count;
        }

    private:
        int count;
        stream<T>* _in;
        void (*_handler)(T* data, int count, void* ctx);
        void* _ctx;

    };

    template <class T>
    class RingBufferSink : public generic_block<RingBufferSink<T>> {
    public:
        RingBufferSink() {}

        RingBufferSink(stream<T>* in) { init(in); }

        ~RingBufferSink() { generic_block<RingBufferSink<T>>::stop(); }

        void init(stream<T>* in) {
            _in = in;
            generic_block<RingBufferSink<T>>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<RingBufferSink<T>>::ctrlMtx);
            generic_block<RingBufferSink<T>>::tempStop();
            _in = in;
            generic_block<RingBufferSink<T>>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }
            if (data.write(_in->data, count) < 0) { return -1; }
            _in->flush();
            return count;
        }

        RingBuffer<T> data;

    private:
        void doStop() {
            _in->stopReader();
            data.stopWriter();
            if (generic_block<RingBufferSink<T>>::workerThread.joinable()) {
                generic_block<RingBufferSink<T>>::workerThread.join();
            }
            _in->clearReadStop();
            data.clearWriteStop();
        }

        int count;
        stream<T>* _in;

    };
}