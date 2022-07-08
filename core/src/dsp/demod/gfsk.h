#pragma once
#include "quadrature.h"
#include "../taps/root_raised_cosine.h"
#include "../filter/fir.h"
#include "../clock_recovery/mm.h"

namespace dsp::demod {
    // Note: I don't like how this demodulator reuses 90% of the code from the PSK demod. Same will be for the PM demod...
    class GFSK : public Processor<complex_t, float> {
        using base_type = Processor<complex_t, float>;
    public:
        GFSK() {}

        GFSK(stream<complex_t>* in, double symbolrate, double samplerate, double deviation, int rrcTapCount, double rrcBeta, double omegaGain, double muGain, double omegaRelLimit = 0.01) {
            init(in, symbolrate, samplerate, deviation, rrcTapCount, rrcBeta, omegaGain, muGain);
        }

        ~GFSK() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            taps::free(rrcTaps);
        }

        void init(stream<complex_t>* in, double symbolrate, double samplerate, double deviation, int rrcTapCount, double rrcBeta, double omegaGain, double muGain, double omegaRelLimit = 0.01) {
            _symbolrate = symbolrate;
            _samplerate = samplerate;
            _deviation = deviation;
            _rrcTapCount = rrcTapCount;
            _rrcBeta = rrcBeta;
            
            demod.init(NULL, _deviation, _samplerate);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            rrc.init(NULL, rrcTaps);
            recov.init(NULL, _samplerate / _symbolrate,  omegaGain, muGain, omegaRelLimit);

            demod.out.free();
            rrc.out.free();
            recov.out.free();

            base_type::init(in);
        }

        void setSymbolrate(double symbolrate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _symbolrate = symbolrate;
            taps::free(rrcTaps);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            rrc.setTaps(rrcTaps);
            recov.setOmega(_samplerate / _symbolrate);
            base_type::tempStart();
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _samplerate = samplerate;
            demod.setDeviation(_deviation, _samplerate);
            taps::free(rrcTaps);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            rrc.setTaps(rrcTaps);
            recov.setOmega(_samplerate / _symbolrate);
            base_type::tempStart();
        }

        void setDeviation(double deviation) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _deviation = deviation;
            demod.setDeviation(_deviation, _samplerate);
        }

        void setRRCParams(int rrcTapCount, double rrcBeta) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _rrcTapCount = rrcTapCount;
            _rrcBeta = rrcBeta;
            taps::free(rrcTaps);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            base_type::tempStart();
        }

        void setRRCTapCount(int rrcTapCount) {
            setRRCParams(rrcTapCount, _rrcBeta);
        }

        void setRRCBeta(int rrcBeta) {
            setRRCParams(_rrcTapCount, rrcBeta);
        }

        void setMMParams(double omegaGain, double muGain, double omegaRelLimit = 0.01) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setOmegaGain(omegaGain);
            recov.setMuGain(muGain);
            recov.setOmegaRelLimit(omegaRelLimit);
        }

        void setOmegaGain(double omegaGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setOmegaGain(omegaGain);
        }

        void setMuGain(double muGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setMuGain(muGain);
        }

        void setOmegaRelLimit(double omegaRelLimit) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setOmegaRelLimit(omegaRelLimit);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            demod.reset();
            rrc.reset();
            recov.reset();
            base_type::tempStart();
        }

        inline int process(int count, complex_t* in, float* out) {
            demod.process(count, in, out);
            rrc.process(count, out, out);
            return recov.process(count, out, out);
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        double _symbolrate;
        double _samplerate;
        double _deviation;
        int _rrcTapCount;
        double _rrcBeta;

        Quadrature demod;
        tap<float> rrcTaps;
        filter::FIR<float, float> rrc;
        clock_recovery::MM<float> recov;
    };
}