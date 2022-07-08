#pragma once
#include "block.h"

namespace dsp {
    template <class A, class B, class O>
    class Operator : public block {
        using base_type = block;
    public:
        Operator() {}

        Operator(stream<A>* a, stream<B>* b) { init(a, b); }

        virtual void init(stream<A>* a, stream<B>* b) {
            _a = a;
            _b = b;
            base_type::registerInput(_a);
            base_type::registerInput(_b);
            base_type::registerOutput(&out);
            base_type::_block_init = true;
        }

        virtual void setInputs(stream<A>* a, stream<B>* b) {
            assert(_block_init);
            std::lock_guard<std::recursive_mutex> lck(ctrlMtx);
            base_type::tempStop();
            base_type::unregisterInput(_a);
            base_type::unregisterInput(_b);
            _a = a;
            _b = b;
            base_type::registerInput(_a);
            base_type::registerInput(_b);
            base_type::tempStart();
        }

        virtual void setInputA(stream<A>* a) {
            assert(_block_init);
            std::lock_guard<std::recursive_mutex> lck(ctrlMtx);
            base_type::tempStop();
            base_type::unregisterInput(_a);
            _a = a;
            base_type::registerInput(_a);
            base_type::tempStart();
        }

        virtual void setInputB(stream<B>* b) {
            assert(_block_init);
            std::lock_guard<std::recursive_mutex> lck(ctrlMtx);
            base_type::tempStop();
            base_type::unregisterInput(_b);
            _b = b;
            base_type::registerInput(_b);
            base_type::tempStart();
        }

        virtual int run() = 0;

        stream<O> out;

    protected:
        stream<A>* _a;
        stream<B>* _b;
    };
}
