#pragma once
#include "../processor.h"

namespace dsp::digital {
    class DifferentialDecoder : public Processor<uint8_t, uint8_t> {
        using base_type = Processor<uint8_t, uint8_t>;
    public:
        DifferentialDecoder() {}

        DifferentialDecoder(stream<uint8_t> *in) { base_type::init(in); }

        void init(stream<uint8_t> *in, uint8_t modulus, uint8_t initSym = 0) {
            _modulus = modulus;
            _initSym = initSym;

            last = _initSym;

            base_type::init(in);
        }

        void setModulus(uint8_t modulus) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _modulus = modulus;
        }

        void setInitSym(uint8_t initSym) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initSym = initSym;
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            last = _initSym;
            base_type::tempStart();
        }
        
        inline int process(int count, const uint8_t* in, uint8_t* out) {
            for (int i = 0; i < count; i++) {
                out[i] = (in[i] - last + _modulus) % _modulus;
                last = in[i];
            }
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        uint8_t last;
        uint8_t _initSym;
        uint8_t _modulus;
    };
}