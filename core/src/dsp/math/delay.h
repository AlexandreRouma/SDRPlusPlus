#pragma once
#include "../processor.h"

namespace dsp::math {
    template<class T>
    class Delay : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        Delay() {}

        Delay(stream<T>* in, int delay) { init(in, delay); }

        ~Delay() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            buffer::free(buffer);
        }

        void init(stream<T>* in, int delay) {
            _delay = delay;

            buffer = buffer::alloc<T>(STREAM_BUFFER_SIZE + 64000);
            bufStart = &buffer[_delay];
            buffer::clear(buffer, _delay);

            base_type::init(in);
        }

        void setDelay(int delay) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _delay = delay;
            bufStart = &buffer[_delay];
            reset();
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            buffer::clear(buffer, _delay);
            base_type::tempStart();
        }

        inline int process(int count, const T* in, T* out) {
            // Copy data into delay buffer
            memcpy(bufStart, in, count * sizeof(T));

            // Copy data out of the delay buffer
            memcpy(out, buffer, count * sizeof(T));

            // Move end of the delay buffer to the front
            memmove(buffer, &buffer[count], _delay * sizeof(T));

            return count;
        }

        virtual int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    private:
        int _delay;
        T* buffer;
        T* bufStart;
    };
}