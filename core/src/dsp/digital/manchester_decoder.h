#pragma once
#include "../processor.h"

namespace dsp::digital {
    class ManchesterDecoder : public Processor<uint8_t, uint8_t> {
        using base_type = Processor<uint8_t, uint8_t>;
    public:
        ManchesterDecoder() {}

        ManchesterDecoder(stream<uint8_t> *in) { base_type::init(in); }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            offset = 0;
            base_type::tempStart();
        }

        inline int process(int count, const uint8_t* in, uint8_t* out) {
            // TODO: NOT THIS BULLSHIT
            int outCount = 0;
            for (; offset < count; offset += 2) {
                out[outCount++] = in[offset];
            }
            offset -= count;
            return outCount;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        int offset = 0;
    };
}