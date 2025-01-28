#include <dsp/sink.h>
#include <dsp/types.h>
#include <dsp/demod/am.h>
#include <dsp/demod/quadrature.h>
#include <dsp/convert/real_to_complex.h>
#include <dsp/channel/frequency_xlator.h>
#include <dsp/filter/fir.h>
#include <dsp/math/delay.h>
#include <dsp/math/conjugate.h>
#include <dsp/channel/rx_vfo.h>
#include "vor_fm_filter.h"
#include <utils/wav.h>

#define VOR_IN_SR   25e3

namespace vor {
    class Receiver : public dsp::Processor<dsp::complex_t, float> {
        using base_type = dsp::Processor<dsp::complex_t, float>;
    public:
        Receiver() {}

        Receiver(dsp::stream<dsp::complex_t>* in) { init(in); }

        ~Receiver() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            dsp::taps::free(fmfTaps);
        }

        void init(dsp::stream<dsp::complex_t>* in) {
            amd.init(NULL, dsp::demod::AM<float>::CARRIER, VOR_IN_SR, 50.0f / VOR_IN_SR, 5.0f / VOR_IN_SR, 100.0f / VOR_IN_SR, VOR_IN_SR);
            amr2c.init(NULL);
            fmr2c.init(NULL);
            fmx.init(NULL, -9960, VOR_IN_SR);
            fmfTaps = dsp::taps::fromArray(FM_TAPS_COUNT, fm_taps);
            fmf.init(NULL, fmfTaps);
            fmd.init(NULL, 600, VOR_IN_SR);
            amde.init(NULL, FM_TAPS_COUNT / 2);
            amv.init(NULL, VOR_IN_SR, 1000, 30, 30);
            fmv.init(NULL, VOR_IN_SR, 1000, 30, 30);

            base_type::init(in);
        }
        
        int process(dsp::complex_t* in, float* out, int count) {
            // Demodulate the AM outer modulation
            volk_32fc_magnitude_32f(amd.out.writeBuf, (lv_32fc_t*)in, count);
            amr2c.process(count, amd.out.writeBuf, amr2c.out.writeBuf);

            // Isolate the FM subcarrier
            fmx.process(count, amr2c.out.writeBuf, fmx.out.writeBuf);
            fmf.process(count, fmx.out.writeBuf, fmx.out.writeBuf);

            // Demodulate the FM subcarrier
            fmd.process(count, fmx.out.writeBuf, fmd.out.writeBuf);
            fmr2c.process(count, fmd.out.writeBuf, fmr2c.out.writeBuf);

            // Delay the AM signal by the same amount as the FM one
            amde.process(count, amr2c.out.writeBuf, amr2c.out.writeBuf);

            // Isolate the 30Hz component on both the AM and FM channels
            int rcount = amv.process(count, amr2c.out.writeBuf, amv.out.writeBuf);
            fmv.process(count, fmr2c.out.writeBuf, fmv.out.writeBuf);

            // If no data was returned, we're done for this round
            if (!rcount) { return 0; }

            // Conjugate FM reference
            volk_32fc_conjugate_32fc((lv_32fc_t*)fmv.out.writeBuf, (lv_32fc_t*)fmv.out.writeBuf, rcount);

            // Multiply both together
            volk_32fc_x2_multiply_32fc((lv_32fc_t*)amv.out.writeBuf, (lv_32fc_t*)amv.out.writeBuf, (lv_32fc_t*)fmv.out.writeBuf, rcount);
            
            // Compute angle
            volk_32fc_s32f_atan2_32f(out, (lv_32fc_t*)amv.out.writeBuf, 1.0f, rcount);

            return rcount;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(base_type::_in->readBuf, base_type::out.writeBuf, count);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    private:
        dsp::demod::AM<float> amd;
        dsp::convert::RealToComplex amr2c;
        dsp::convert::RealToComplex fmr2c;
        dsp::channel::FrequencyXlator fmx;
        dsp::tap<float> fmfTaps;
        dsp::filter::FIR<dsp::complex_t, float> fmf;
        dsp::demod::Quadrature fmd;
        dsp::math::Delay<dsp::complex_t> amde;
        dsp::channel::RxVFO amv;
        dsp::channel::RxVFO fmv;
    };
}