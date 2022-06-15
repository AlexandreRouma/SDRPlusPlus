#pragma once
#include "../sink.h"

namespace dsp::sink {
    template <class T>
    class Handler : public Sink<T> {
        using base_type = Sink<T>;
    public:
        Handler() {}

        Handler(stream<T>* in, void (*handler)(T* data, int count, void* ctx), void* ctx) { init(in, handler, ctx); }

        void init(stream<T>* in, void (*handler)(T* data, int count, void* ctx), void* ctx) {
            _handler = handler;
            _ctx = ctx;
            base_type::init(in);
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            _handler(_in->readBuf, count, _ctx);

            base_type::_in->flush();
            return count;
        }

    protected:
        void (*_handler)(T* data, int count, void* ctx);
        void* _ctx;

    };
}