#pragma once
#include "frequency_xlator.h"
#include "../multirate/rational_resampler.h"

namespace dsp::channel {
    class RxVFO : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        RxVFO() {}

        RxVFO(stream<complex_t>* in, double inSamplerate, double outSamplerate, double bandwidth, double offset) { init(in, inSamplerate, outSamplerate, bandwidth, offset); }

        void init(stream<complex_t>* in, double inSamplerate, double outSamplerate, double bandwidth, double offset) {
            _inSamplerate = inSamplerate;
            _outSamplerate = outSamplerate;
            _bandwidth = bandwidth;
            _offset = offset;
            filterNeeded = (_bandwidth != _outSamplerate);
            ftaps.taps = NULL;

            xlator.init(NULL, -_offset, _inSamplerate);
            resamp.init(NULL, _inSamplerate, _outSamplerate);
            generateTaps();
            filter.init(NULL, ftaps);

            base_type::init(in);
        }

        void setInSamplerate(double inSamplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _inSamplerate = inSamplerate;
            xlator.setOffset(-_offset, _inSamplerate);
            resamp.setInSamplerate(_inSamplerate);
            base_type::tempStart();
        }

        void setOutSamplerate(double outSamplerate, double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _outSamplerate = outSamplerate;
            _bandwidth = bandwidth;
            filterNeeded = (_bandwidth != _outSamplerate);
            resamp.setOutSamplerate(_outSamplerate);
            if (filterNeeded) {
                generateTaps();
                filter.setTaps(ftaps);
            }
            base_type::tempStart();
        }

        void setBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _bandwidth = bandwidth;
            filterNeeded = (_bandwidth != _outSamplerate);
            if (filterNeeded) {
                generateTaps();
                filter.setTaps(ftaps);
            }
            base_type::tempStart();
        }

        void setOffset(double offset) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _offset = offset;
            xlator.setOffset(-_offset, _inSamplerate);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            xlator.reset();
            resamp.reset();
            filter.reset();
            base_type::tempStart();
        }

        inline int process(int count, const complex_t* in, complex_t* out) {
            xlator.process(count, in, out);
            if (!filterNeeded) {
                return resamp.process(count, out, out);
            }
            count = resamp.process(count, out, out);
            filter.process(count, out, out);
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        void generateTaps() {
            taps::free(ftaps);
            double filterWidth = _bandwidth / 2.0;
            ftaps = taps::lowPass(filterWidth, filterWidth * 0.1, _outSamplerate);
            printf("New taps just dropped: %lf %lf %lf\n", filterWidth, filterWidth*0.1, _outSamplerate);
        }

        FrequencyXlator xlator;
        multirate::RationalResampler<complex_t> resamp;
        filter::FIR<complex_t, float> filter;
        tap<float> ftaps;
        bool filterNeeded;

        double _inSamplerate;
        double _outSamplerate;
        double _bandwidth;
        double _offset;
    };
}