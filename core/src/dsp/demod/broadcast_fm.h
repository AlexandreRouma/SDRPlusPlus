#pragma once
#include "fm.h"
#include "../taps/band_pass.h"
#include "../filter/fir.h"
#include "../loop/pll.h"
#include "../convert/l_r_to_stereo.h"
#include "../convert/real_to_complex.h"
#include "../convert/complex_to_real.h"
#include "../math/conjugate.h"
#include "../math/delay.h"
#include "../math/multiply.h"
#include "../math/add.h"
#include "../math/subtract.h"

namespace dsp::demod {
    class BroadcastFM : public Processor<complex_t, stereo_t> {
        using base_type = Processor<complex_t, stereo_t>;
    public:
        BroadcastFM() {}

        BroadcastFM(stream<complex_t>* in, double deviation, double samplerate, bool stereo = true) { init(in, deviation, samplerate, stereo); }

        ~BroadcastFM() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            buffer::free(lmr);
            buffer::free(l);
            buffer::free(r);
        }

        virtual void init(stream<complex_t>* in, double deviation, double samplerate, bool stereo = true) {
            _deviation = deviation;
            _samplerate = samplerate;
            _stereo = stereo;
            
            demod.init(NULL, _deviation, _samplerate);
            pilotFirTaps = taps::bandPass<complex_t>(18750.0, 19250.0, 3000.0, _samplerate);
            pilotFir.init(NULL, pilotFirTaps);
            rtoc.init(NULL);
            pilotPLL.init(NULL, 0.1/*TODO: adapt to samplerate*/, 0.0, math::freqToOmega(19000.0, _samplerate), math::freqToOmega(18750.0, _samplerate), math::freqToOmega(19250.0, _samplerate));
            delay.init(NULL, pilotFirTaps.size / 2.0);

            lmr = buffer::alloc<float>(STREAM_BUFFER_SIZE);
            l = buffer::alloc<float>(STREAM_BUFFER_SIZE);
            r = buffer::alloc<float>(STREAM_BUFFER_SIZE);

            base_type::init(in);
        }

        void setDeviation(double deviation) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _deviation = deviation;
            demod.setDeviation(_deviation, _samplerate);
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _samplerate = samplerate;

            demod.setDeviation(_deviation, _samplerate);
            taps::free(pilotFirTaps);
            pilotFirTaps = taps::bandPass<complex_t>(18750.0, 19250.0, 3000.0, samplerate);
            pilotFir.setTaps(pilotFirTaps);
            pilotPLL.setFrequencyLimits(math::freqToOmega(18750.0, _samplerate), math::freqToOmega(19250.0, _samplerate));
            pilotPLL.setInitialFreq(math::freqToOmega(19000.0, _samplerate));
            delay.setDelay(pilotFirTaps.size / 2);

            reset();
            base_type::tempStart();
        }

        void setStereo(bool stereo) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _stereo = stereo;
            reset();
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            demod.reset();
            pilotFir.reset();
            pilotPLL.reset();
            delay.reset();
            base_type::tempStart();
        }

        inline int process(int count, complex_t* in, stereo_t* out) {
            // Demodulate
            demod.process(count, in, demod.out.writeBuf);
            if (_stereo) {
                // Convert to complex
                rtoc.process(count, demod.out.writeBuf, rtoc.out.writeBuf);

                // Filter out pilot and run through PLL
                pilotFir.process(count, rtoc.out.writeBuf, pilotFir.out.writeBuf);
                pilotPLL.process(count, pilotFir.out.writeBuf, pilotPLL.out.writeBuf);

                // Conjugate PLL output to down convert the L-R signal
                math::Conjugate::process(count, pilotPLL.out.writeBuf, pilotPLL.out.writeBuf);
                math::Multiply<dsp::complex_t>::process(count, rtoc.out.writeBuf, pilotPLL.out.writeBuf, rtoc.out.writeBuf);

                // Convert output back to real for further processing
                convert::ComplexToReal::process(count, rtoc.out.writeBuf, lmr);

                // Do L = (L+R) + (L-R), R = (L+R) - (L-R)
                math::Add<float>::process(count, demod.out.writeBuf, lmr, l);
                math::Subtract<float>::process(count, demod.out.writeBuf, lmr, r);

                // Interleave into stereo
                convert::LRToStereo::process(count, l, r, out);
            }
            else {
                // Interleave raw MPX to stereo
                convert::LRToStereo::process(count, demod.out.writeBuf, demod.out.writeBuf, out);
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
        double _deviation;
        double _samplerate;
        bool _stereo;

        FM demod;
        tap<complex_t> pilotFirTaps;
        filter::FIR<complex_t, complex_t> pilotFir;
        convert::RealToComplex rtoc;
        loop::PLL pilotPLL;
        math::Delay<float> delay;

        float* lmr;
        float* l;
        float* r;
        
    };
}