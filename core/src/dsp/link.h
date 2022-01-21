#pragma once
#include <dsp/block.h>

namespace dsp {
    template <class T>
    class Link : public generic_block<Link<T>> {
    public:
        Link() {}

        Link(stream<T>* a, stream<T>* b) { init(a, b); }

        void init(stream<T>* in, stream<T>* out) {
            _in = in;
            _out = out;
            generic_block<Link<T>>::registerInput(_in);
            generic_block<Link<T>>::registerOutput(_out);
            generic_block<Link<T>>::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(generic_block<Link<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Link<T>>::ctrlMtx);
            generic_block<Link<T>>::tempStop();
            generic_block<Link<T>>::unregisterInput(_in);
            _in = in;
            generic_block<Link<T>>::registerInput(_in);
            generic_block<Link<T>>::tempStart();
        }

        void setOutput(stream<T>* out) {
            assert(generic_block<Link<T>>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Link<T>>::ctrlMtx);
            generic_block<Link<T>>::tempStop();
            generic_block<Link<T>>::unregisterOutput(_out);
            _out = out;
            generic_block<Link<T>>::registerOutput(_out);
            generic_block<Link<T>>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            memcpy(_out->writeBuf, _in->readBuf, count * sizeof(T));

            _in->flush();
            return _out->swap(count) ? count : -1;
        }

    private:
        stream<T>* _in;
        stream<T>* _out;
    };
}