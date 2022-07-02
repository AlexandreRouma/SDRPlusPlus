#pragma once
#include "../sink.h"

namespace dsp::routing {
    template <class T>
    class Doubler : public Sink<T> {
        using base_type = Sink<T>;
    public:
        Doubler() {}

        Doubler(stream<T>* in) { init(in); }

        void init(stream<T>* in) {
            base_type::registerOutput(&outA);
            base_type::registerOutput(&outB);
            base_type::init(in);
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            memcpy(outA.writeBuf, base_type::_in->readBuf, count * sizeof(T));
            memcpy(outB.writeBuf, base_type::_in->readBuf, count * sizeof(T));
            if (!outA.swap(count)) {
                base_type::_in->flush();
                return -1;
            }
            if (!outB.swap(count)) {
                base_type::_in->flush();
                return -1;
            }

            base_type::_in->flush();

            return count;
        }

        stream<T> outA;
        stream<T> outB;

    protected:

    };
}