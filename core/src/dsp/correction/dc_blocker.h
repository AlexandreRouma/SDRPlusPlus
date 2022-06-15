#pragma once
#include "../processor.h"

namespace dsp::correction {
    template<class T>
    class DCBlocker : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        DCBlocker() {}

        DCBlocker(stream<T>* in, double rate) { init(in, rate); }

        DCBlocker(stream<T>* in, double rate, double samplerate) { init(in, rate, samplerate); }

        void init(stream<T>* in, double rate) {
            _rate = rate;
            reset();
            base_type::init(in);
        }

        void init(stream<T>* in, double rate, double samplerate) {
            init(in, rate / samplerate);
        }

        void setRate(double rate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _rate = rate;
        }

        void setRate(double rate, double samplerate)  {
            setRate(rate / samplerate);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            if constexpr (std::is_same_v<T, float>) {
                offset = 0.0f;
            }
            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                offset = { 0.0f, 0.0f };
            }
            base_type::tempStart();
        }

        // TODO: Add back the const
        int process(int count, T* in, T* out) {
            for (int i = 0; i < count; i++) {
                out[i] = in[i] - offset;
                offset += out[i] * _rate;
            }
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

    protected:
        float _rate;
        T offset;
    };
}