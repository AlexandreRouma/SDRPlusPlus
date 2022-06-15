#pragma once
#include "block.h"

namespace dsp {
    template <class T>
    class Sink : public block {
    public:
        Sink() {}

        Sink(stream<T>* in) { init(in); }

        virtual ~Sink() {}

        virtual void init(stream<T>* in) {
            _in = in;
            registerInput(_in);
            _block_init = true;
        }

        virtual void setInput(stream<T>* in) {
            assert(_block_init);
            std::lock_guard<std::recursive_mutex> lck(ctrlMtx);
            tempStop();
            unregisterInput(_in);
            _in = in;
            registerInput(_in);
            tempStart();
        }

        virtual int run() = 0;

    protected:
        stream<T>* _in;
    };
}
