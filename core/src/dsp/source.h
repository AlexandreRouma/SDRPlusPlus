#pragma once
#include "block.h"

namespace dsp {
    template <class T>
    class Source : public block {
    public:
        Source() {}

        Source() { init(); }

        virtual ~Source() {}

        virtual void init() {
            registerOutput(&out);
            _block_init = true;
        }

        virtual int run() = 0;

        stream<T> out;
    };
}
