#pragma once
#include "../processor.h"

namespace dsp::loop {
    template <class T>
    class FastAGC : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        FastAGC() {}

        FastAGC(stream<T>* in, double setPoint, double maxGain, double rate, double initGain = 1.0) { init(in, setPoint, maxGain, rate, initGain); }

        void init(stream<T>* in, double setPoint, double maxGain, double rate, double initGain = 1.0) {
            _setPoint = setPoint;
            _maxGain = maxGain;
            _rate = rate;
            _initGain = initGain;

            _gain = _initGain;

            base_type::init(in);
        }

        void setSetPoint(double setPoint) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _setPoint = setPoint;
        }

        void setMaxGain(double maxGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _maxGain = maxGain;
        }

        void setRate(double rate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _rate = rate;
        }

        void setInitGain(double initGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initGain = initGain;
        }

        void setGain(double gain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _gain = gain;
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _gain = _initGain;
        }

        inline int process(int count, T* in, T* out) {
            for (int i = 0; i < count; i++) {
                // Output scaled input
                out[i] = in[i] * _gain;

                // Calculate output amplitude
                float amp;
                if constexpr (std::is_same_v<T, float>) {
                    amp = fabsf(out[i]);
                }
                if constexpr (std::is_same_v<T, complex_t>) {
                    amp = out[i].amplitude();
                }

                // Update and clamp gain
                _gain += (_setPoint - amp) * _rate;
                if (_gain > _maxGain) { _gain = _maxGain; }
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
        float _gain;
        float _setPoint;
        float _rate;
        float _maxGain;
        float _initGain;

    };
}