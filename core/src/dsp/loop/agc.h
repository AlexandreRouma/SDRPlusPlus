#pragma once
#include "../processor.h"

namespace dsp::loop {
    template <class T>
    class AGC : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        AGC() {}

        AGC(stream<T>* in) { init(in); }

        void init(stream<T>* in, double setPoint, double rate, double initGain = 1.0) {
            _setPoint = setPoint;
            _rate = rate;
            _initGain = initGain;
            gain = _initGain;
            base_type::init(in);
        }

        void setSetPoint(double setPoint) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _setPoint = setPoint;
        }

        void setRate(double rate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _rate = rate;
        }

        void setInitialGain(double initGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initGain = initGain;
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            gain = _initGain;
        }

        inline int process(int count, T* in, T* out) {
            for (int i = 0; i < count; i++) {
                // Scale output by gain
                out[i] = in[i] * gain;

                // Update gain according to setpoint and rate
                if constexpr (std::is_same_v<T, complex_t>) {
                    gain += (_setPoint - out[i].amplitude()) * _rate;
                }
                if constexpr (std::is_same_v<T, float>) {
                    gain += (_setPoint - fabsf(out[i])) * _rate;
                }
            }
            printf("%f\n", gain);
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
        float _setPoint;
        float _rate;
        float _initGain;

        float gain;

    };
}