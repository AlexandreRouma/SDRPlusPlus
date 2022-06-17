#pragma once
#include "../processor.h"

namespace dsp::loop {
    template <class T>
    class AGC : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        AGC() {}

        AGC(stream<T>* in, double setPoint, double rate, double maxGain, double maxOutputAmp, double initGain = 1.0) { init(in, setPoint, rate, maxGain, maxOutputAmp, initGain); }

        void init(stream<T>* in, double setPoint, double rate, double maxGain, double maxOutputAmp, double initGain = 1.0) {
            _setPoint = setPoint;
            _rate = rate;
            _invRate = 1.0f - _rate;
            _maxGain = maxGain;
            _maxOutputAmp = maxOutputAmp;
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
            _invRate = 1.0f - _rate;
        }

        void setMaxGain(double maxGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _maxGain = maxGain;
        }

        void setMaxOutputAmp(double maxOutputAmp) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _maxOutputAmp = maxOutputAmp;
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
            amp = 1.0f;
        }

        inline int process(int count, T* in, T* out) {
            for (int i = 0; i < count; i++) {
                // Get signal amplitude
                float inAmp;
                if constexpr (std::is_same_v<T, complex_t>) {
                    inAmp = in[i].amplitude();
                }
                if constexpr (std::is_same_v<T, float>) {
                    inAmp = fabsf(in[i]);
                }

                // Update average amplitude
                if (inAmp != 0.0f) {
                    amp = (amp * _invRate) + (inAmp * _rate);
                    gain = std::min<float>(_setPoint / amp, _maxGain);
                }

                // Scale output by gain
                out[i] = in[i] * gain;
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
        float _setPoint;
        float _rate;
        float _invRate;
        float _maxGain;
        float _maxOutputAmp;
        float _initGain;

        float gain;
        float amp = 1.0;

    };
}