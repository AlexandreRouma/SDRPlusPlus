#pragma once
#include "../sink.h"

namespace dsp::routing {
    template <class T>
    class StreamLink : public Sink<T> {
        using base_type = Sink<T>;
    public:
        StreamLink() {}

        StreamLink(stream<T>* in, stream<T>* out) { init(in, out); }

        void init(stream<T>* in, stream<T>* out) {
            _out = out;
            base_type::registerOutput(_out);
            base_type::init(in);
        }

        void setOutput(stream<T>* out) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            base_type::unregisterOutput(_out);
            _out = out;
            base_type::registerOutput(_out);
            base_type::tempStart();
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            memcpy(_out->writeBuf, base_type::_in->readBuf, count * sizeof(T));

            base_type::_in->flush();
            if (!_out->swap(count)) { return -1; }
            return count;
        }

    protected:
        stream<T>* _out;

    };
}