#pragma once
#include "../processor.h"
#include "../math/phasor.h"
#include "../math/normalize_phase.h"
#include "../math/hz_to_rads.h"

namespace dsp::mod {
    class Quadrature : Processor<float, complex_t> {
        using base_type = Processor<float, complex_t>;
    public:
        Quadrature() {}

        Quadrature(stream<float>* in, double deviation) { init(in, deviation); }

        Quadrature(stream<float>* in, double deviation, double samplerate) { init(in, deviation, samplerate); }

        void init(stream<float>* in, double deviation) {
            _deviation = deviation;
            base_type::init(in);
        }

        void init(stream<float>* in, double deviation, double samplerate) {
            init(in, math::hzToRads(deviation, samplerate));
        }

        void setDeviation(double deviation) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _deviation = deviation;
        }

        void setDeviation(double deviation, double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _deviation = math::hzToRads(deviation, samplerate);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            phase = 0.0f;
        }

        inline int process(int count, const float* in, complex_t* out) {
            for (int i = 0; i < count; i++) {
                phase = math::normalizePhase(phase + (_deviation * in[i]));
                out[i] = math::phasor(phase);
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

    private:
        float _deviation;
        float phase = 0.0f;
    };
}