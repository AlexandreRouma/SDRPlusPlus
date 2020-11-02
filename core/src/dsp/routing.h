#pragma once
#include <dsp/block.h>
#include <cstring>

namespace dsp {
    template <class T>
    class Splitter : public generic_block<Splitter<T>> {
    public:
        Splitter() {}

        Splitter(stream<T>* in) { init(in); }

        ~Splitter() { generic_block<Splitter>::stop(); }

        void init(stream<T>* in) {
            _in = in;
            generic_block<Splitter>::registerInput(_in);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            generic_block<Splitter>::unregisterInput(_in);
            _in = in;
            generic_block<Splitter>::registerInput(_in);
            generic_block<Splitter>::tempStart();
        }

        void bindStream(stream<T>* stream) {
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            out.push_back(stream);
            generic_block<Splitter>::registerOutput(stream);
            generic_block<Splitter>::tempStart();
        }

        void unbindStream(stream<T>* stream) {
            std::lock_guard<std::mutex> lck(generic_block<Splitter>::ctrlMtx);
            generic_block<Splitter>::tempStop();
            generic_block<Splitter>::unregisterOutput(stream);
            out.erase(std::remove(out.begin(), out.end(), stream), out.end());
            generic_block<Splitter>::tempStart();
        }

    private:
        int run() {
            // TODO: If too slow, buffering might be necessary
            int count = _in->read();
            if (count < 0) { return -1; }
            for (const auto& stream : out) {
                if (stream->aquire() < 0) { return -1; } 
                memcpy(stream->data, _in->data, count * sizeof(T));
                stream->write(count);
            }
            _in->flush();
            return count;
        }

        stream<T>* _in;
        std::vector<stream<T>*> out;

    };

    template <class T>
    class Reshaper : public generic_block<Reshaper<T>> {
    public:
        Reshaper() {}

        Reshaper(stream<T>* in) { init(in); }

        ~Reshaper() { generic_block<Reshaper<T>>::stop(); }

        void init(stream<T>* in) {
            _in = in;
            buffer = (T*)volk_malloc(STREAM_BUFFER_SIZE * sizeof(T), volk_get_alignment());
            generic_block<Reshaper<T>>::registerInput(_in);
            generic_block<Reshaper<T>>::registerOutput(&out);
        }

        void setInput(stream<T>* in) {
            std::lock_guard<std::mutex> lck(generic_block<Reshaper<T>>::ctrlMtx);
            generic_block<Reshaper<T>>::tempStop();
            _in = in;
            generic_block<Reshaper<T>>::tempStart();
        }

        int run() {
            int count = _in->read();
            _in->flush();
            return count;
        }

        stream<T> out;

    private:
        stream<T>* _in;
        T* buffer;
        int _outBlockSize;
        int readCount;

    };
}