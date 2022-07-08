#pragma once
#include "../processor.h"


namespace dsp::filter {
    template<class T>
    class Deemphasis : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        Deemphasis() {}

        Deemphasis(stream<T>* in, double tau, double samplerate) {}

        void init(stream<T>* in, double tau, double samplerate) {
            _tau = tau;
            _samplerate = samplerate;

            updateAlpha();

            // Initialize state
            if constexpr (std::is_same_v<T, float>) {
                lastOut = 0;
            }
            if constexpr (std::is_same_v<T, stereo_t>) {
                lastOut = { 0, 0 };
            }

            base_type::init(in);
        }

        void setTau(double tau) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _tau = tau;
            updateAlpha();
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _samplerate = samplerate;
            updateAlpha();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            if constexpr (std::is_same_v<T, float>) {
                lastOut = 0;
            }
            if constexpr (std::is_same_v<T, stereo_t>) {
                lastOut = { 0, 0 };
            }
            base_type::tempStart();
        }

        inline int process(int count, const T* in, T* out) {
            if constexpr (std::is_same_v<T, float>) {
                out[0] = (alpha * in[0]) + ((1 - alpha) * lastOut);
                for (int i = 1; i < count; i++) {
                    out[i] = (alpha * in[i]) + ((1 - alpha) * out[i - 1]);
                }
                lastOut = out[count - 1];
            }
            if constexpr (std::is_same_v<T, stereo_t>) {
                out[0].l = (alpha * in[0].l) + ((1 - alpha) * lastOut.l);
                out[0].r = (alpha * in[0].r) + ((1 - alpha) * lastOut.r);
                for (int i = 1; i < count; i++) {
                    out[i].l = (alpha * in[i].l) + ((1 - alpha) * out[i - 1].l);
                    out[i].r = (alpha * in[i].r) + ((1 - alpha) * out[i - 1].r);
                }
                lastOut.l = out[count - 1].l;
                lastOut.r = out[count - 1].r;
            }
            return count;
        }

        //DEFAULT_PROC_RUN();

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }
            process(count, base_type::_in->readBuf, base_type::out.writeBuf);
            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    private:
        void updateAlpha() {
            float dt = 1.0f / _samplerate;
            alpha = dt / (_tau + dt);
        }

        double _tau;
        double _samplerate;

        float alpha;
        T lastOut;
    };
}