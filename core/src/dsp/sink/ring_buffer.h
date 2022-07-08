#pragma once
#include "../sink.h"
#include "../buffer/ring_buffer.h"

// NOTE: THIS IS COMPLETELY UNTESTED AND PROBABLY BROKEN!!!

namespace dsp::sink {
    template <class T>
    class RingBuffer : public Sink<T> {
        using base_type = Sink<T>;
    public:
        RingBuffer() {}

        RingBuffer(stream<T>* in, int maxLatency) { init(in, maxLatency); }

        void init(stream<T>* in, int maxLatency) {
            data.init(maxLatency);
            base_type::init(in);
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            if (data.write(base_type::_in->readBuf, count) < 0) { return -1; }

            base_type::_in->flush();
            return count;
        }

        buffer::RingBuffer<T> data;

    private:
        void doStop() {
            base_type::_in->stopReader();
            data.stopWriter();
            if (base_type::workerThread.joinable()) {
                base_type::workerThread.join();
            }
            base_type::_in->clearReadStop();
            data.clearWriteStop();
        }
    };
}