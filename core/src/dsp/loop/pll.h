#pragma once
#include "../processor.h"
#include "../math/norm_phase_diff.h"
#include "../math/phasor.h"
#include "phase_control_loop.h"

namespace dsp::loop {
    class PLL : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        PLL() {}

        PLL(stream<complex_t>* in, double bandwidth, double initPhase = 0.0, double initFreq = 0.0, double minFreq = -FL_M_PI, double maxFreq = FL_M_PI) { init(in, bandwidth, initFreq, initPhase, minFreq, maxFreq); }

        void init(stream<complex_t>* in, double bandwidth, double initPhase = 0.0, double initFreq = 0.0, double minFreq = -FL_M_PI, double maxFreq = FL_M_PI) {
            _initPhase = initPhase;
            _initFreq = initFreq;

            // Init phase control loop
            float alpha, beta;
            PhaseControlLoop<float>::criticallyDamped(bandwidth, alpha, beta);
            pcl.init(alpha, beta, initPhase, -FL_M_PI, FL_M_PI, initFreq, minFreq, maxFreq);
            
            base_type::init(in);
        }

        void setInitialPhase(double initPhase) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initPhase = initPhase;
        }

        void setInitialFreq(double initFreq) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _initFreq = initFreq;
        }

        void setFrequencyLimits(double minFreq, double maxFreq) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            pcl.setFreqLimits(minFreq, maxFreq);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            pcl.phase = _initPhase;
            pcl.freq = _initFreq;
            base_type::tempStart();
        }

        inline int process(int count, complex_t* in, complex_t* out) {
            for (int i = 0; i < count; i++) {
                out[i] = math::phasor(pcl.phase);
                pcl.advance(math::normPhaseDiff(in[i].phase() - pcl.phase));
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
        PhaseControlLoop<float> pcl;
        float _initPhase;
        float _initFreq;
        complex_t lastVCO = { 1.0f, 0.0f };

    };
}