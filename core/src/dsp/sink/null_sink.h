#pragma once
#include "../sink.h"

namespace dsp::sink {
    template <class T>
    class Null : public Sink<T> {
        using base_type = Sink<T>;
    public:
        Null() {}

        Null(stream<T>* in, void (*handler)(T* data, int count, void* ctx), void* ctx) { base_type::init(in); }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }
            base_type::_in->flush();
            return count;
        }
    };
}