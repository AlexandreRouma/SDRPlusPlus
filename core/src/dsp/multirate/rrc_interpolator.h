#pragma once
#include "../multirate/polyphase_resampler.h"
#include "../taps/root_raised_cosine.h"
#include <numeric>

namespace dsp::multirate {
    template <class T>
    class RRCInterpolator : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        RRCInterpolator() {}

        RRCInterpolator(stream<T>* in, double symbolrate, double samplerate, double rrcBeta, int rrcTapCount) { init(in, symbolrate, samplerate, rrcBeta, rrcTapCount); }

        void init(stream<T>* in, double symbolrate, double samplerate, double rrcBeta, int rrcTapCount) {
            _symbolrate = symbolrate;
            _samplerate = samplerate;
            _rrcTapCount = rrcTapCount;

            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, rrcBeta, _symbolrate, _samplerate);
            resamp.init(NULL, 1, 1, rrcTaps);
            resamp.out.free();
            genTaps();

            base_type::init(in);
        }

        void setRates(double symbolrate, double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _symbolrate = symbolrate;
            _samplerate = samplerate;
            genTaps();
            base_type::tempStart();
        }

        void setRRCParam(double rrcBeta, int rrcTapCount) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _rrcBeta = rrcBeta;
            _rrcTapCount = rrcTapCount;
            genTaps();
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            resamp.reset();
            base_type::tempStart();
        }

        inline int process(int count, const T* in, T* out) {
            return resamp.process(count, in, out);
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

    private:
        void genTaps() {
            // Free current taps if they exist
            taps::free(rrcTaps);

            // Calculate the rational samplerate ratio
            int InSR = round(_symbolrate);
            int OutSR = round(_samplerate);
            int gcd = std::gcd(InSR, OutSR);
            int interp = OutSR / gcd;
            int decim = InSR / gcd;

            // Configure resampler
            double tapSamplerate = _symbolrate * (double)interp;
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount * interp, _rrcBeta, _symbolrate, tapSamplerate);
            resamp.setRatio(interp, decim, rrcTaps);
        }

        double _symbolrate;
        double _samplerate;
        double _rrcBeta;
        int _rrcTapCount;

        tap<float> rrcTaps;
        multirate::PolyphaseResampler<T> resamp;

    };
}