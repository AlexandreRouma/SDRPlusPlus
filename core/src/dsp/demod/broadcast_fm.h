#pragma once
#include "quadrature.h"
#include "../taps/low_pass.h"
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
#include "../multirate/rational_resampler.h"

namespace dsp::demod {
    class BroadcastFM : public Processor<complex_t, stereo_t> {
        using base_type = Processor<complex_t, stereo_t>;
    public:
        BroadcastFM() {}

        BroadcastFM(stream<complex_t>* in, double deviation, double samplerate, bool stereo = true, bool lowPass = true, bool rdsOut = false) { init(in, deviation, samplerate, stereo, lowPass); }

        ~BroadcastFM() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            buffer::free(lmr);
            buffer::free(l);
            buffer::free(r);
            taps::free(pilotFirTaps);
            taps::free(audioFirTaps);
        }

        virtual void init(stream<complex_t>* in, double deviation, double samplerate, bool stereo = true, bool lowPass = true, bool rdsOut = false) {
            _deviation = deviation;
            _samplerate = samplerate;
            _stereo = stereo;
            _lowPass = lowPass;
            _rdsOut = rdsOut;
            
            demod.init(NULL, _deviation, _samplerate);
            pilotFirTaps = taps::bandPass<complex_t>(18750.0, 19250.0, 3000.0, _samplerate, true);
            pilotFir.init(NULL, pilotFirTaps);
            rtoc.init(NULL);
            pilotPLL.init(NULL, 25000.0 / _samplerate, 0.0, math::freqToOmega(19000.0, _samplerate), math::freqToOmega(18750.0, _samplerate), math::freqToOmega(19250.0, _samplerate));
            lprDelay.init(NULL, ((pilotFirTaps.size - 1) / 2) + 1);
            lmrDelay.init(NULL, ((pilotFirTaps.size - 1) / 2) + 1);
            audioFirTaps = taps::lowPass(15000.0, 4000.0, _samplerate);
            alFir.init(NULL, audioFirTaps);
            arFir.init(NULL, audioFirTaps);
            rdsResamp.init(NULL, samplerate, 5000.0);

            lmr = buffer::alloc<float>(STREAM_BUFFER_SIZE);
            l = buffer::alloc<float>(STREAM_BUFFER_SIZE);
            r = buffer::alloc<float>(STREAM_BUFFER_SIZE);

            lprDelay.out.free();
            lmrDelay.out.free();
            arFir.out.free();
            alFir.out.free();
            rdsResamp.out.free();

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
            pilotFirTaps = taps::bandPass<complex_t>(18750.0, 19250.0, 3000.0, samplerate, true);
            pilotFir.setTaps(pilotFirTaps);
            
            pilotPLL.setFrequencyLimits(math::freqToOmega(18750.0, _samplerate), math::freqToOmega(19250.0, _samplerate));
            pilotPLL.setInitialFreq(math::freqToOmega(19000.0, _samplerate));
            lprDelay.setDelay(((pilotFirTaps.size - 1) / 2) + 1);
            lmrDelay.setDelay(((pilotFirTaps.size - 1) / 2) + 1);

            taps::free(audioFirTaps);
            audioFirTaps = taps::lowPass(15000.0, 4000.0, _samplerate);
            alFir.setTaps(audioFirTaps);
            arFir.setTaps(audioFirTaps);

            rdsResamp.setInSamplerate(samplerate);

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

        void setLowPass(bool lowPass) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _lowPass = lowPass;
            reset();
            base_type::tempStart();
        }

        void setRDSOut(bool rdsOut) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _rdsOut = rdsOut;
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
            lprDelay.reset();
            lmrDelay.reset();
            alFir.reset();
            arFir.reset();
            base_type::tempStart();
        }

        inline int process(int count, complex_t* in, stereo_t* out, int& rdsOutCount, float* rdsout = NULL) {
            // Demodulate
            demod.process(count, in, demod.out.writeBuf);
            if (_stereo) {
                // Convert to complex
                rtoc.process(count, demod.out.writeBuf, rtoc.out.writeBuf);

                // Filter out pilot and run through PLL
                pilotFir.process(count, rtoc.out.writeBuf, pilotFir.out.writeBuf);
                pilotPLL.process(count, pilotFir.out.writeBuf, pilotPLL.out.writeBuf);

                // Delay
                lprDelay.process(count, demod.out.writeBuf, demod.out.writeBuf);
                lmrDelay.process(count, rtoc.out.writeBuf, rtoc.out.writeBuf);
                
                // conjugate PLL output to down convert twice the L-R signal
                math::Conjugate::process(count, pilotPLL.out.writeBuf, pilotPLL.out.writeBuf);
                math::Multiply<dsp::complex_t>::process(count, rtoc.out.writeBuf, pilotPLL.out.writeBuf, rtoc.out.writeBuf);
                math::Multiply<dsp::complex_t>::process(count, rtoc.out.writeBuf, pilotPLL.out.writeBuf, rtoc.out.writeBuf);

                // Do RDS demod
                if (_rdsOut) {
                    // Since the PLL output is no longer needed after this, use it as the output
                    math::Multiply<dsp::complex_t>::process(count, rtoc.out.writeBuf, pilotPLL.out.writeBuf, pilotPLL.out.writeBuf);
                    convert::ComplexToReal::process(count, pilotPLL.out.writeBuf, rdsout);
                    volk_32f_s32f_multiply_32f(rdsout, rdsout, 100.0, count);
                    rdsOutCount = rdsResamp.process(count, rdsout, rdsout);
                }

                // Convert output back to real for further processing
                convert::ComplexToReal::process(count, rtoc.out.writeBuf, lmr);

                // Amplify by 2x
                volk_32f_s32f_multiply_32f(lmr, lmr, 2.0f, count);

                // Do L = (L+R) + (L-R), R = (L+R) - (L-R)
                math::Add<float>::process(count, demod.out.writeBuf, lmr, l);
                math::Subtract<float>::process(count, demod.out.writeBuf, lmr, r);

                // Filter if needed
                if (_lowPass) {
                    alFir.process(count, l, l);
                    arFir.process(count, r, r);
                }

                // Interleave into stereo
                convert::LRToStereo::process(count, l, r, out);
            }
            else {
                // Process RDS if needed. Note: find a way to not have to copy half the code from the stereo demod
                if (_rdsOut) {
                    // Convert to complex
                    rtoc.process(count, demod.out.writeBuf, rtoc.out.writeBuf);

                    // Filter out pilot and run through PLL
                    pilotFir.process(count, rtoc.out.writeBuf, pilotFir.out.writeBuf);
                    pilotPLL.process(count, pilotFir.out.writeBuf, pilotPLL.out.writeBuf);

                    // Delay
                    lprDelay.process(count, demod.out.writeBuf, demod.out.writeBuf);
                    lmrDelay.process(count, rtoc.out.writeBuf, rtoc.out.writeBuf);
                    
                    // conjugate PLL output to down convert twice the L-R signal
                    math::Conjugate::process(count, pilotPLL.out.writeBuf, pilotPLL.out.writeBuf);
                    math::Multiply<dsp::complex_t>::process(count, rtoc.out.writeBuf, pilotPLL.out.writeBuf, rtoc.out.writeBuf);
                    math::Multiply<dsp::complex_t>::process(count, rtoc.out.writeBuf, pilotPLL.out.writeBuf, rtoc.out.writeBuf);

                    // Since the PLL output is no longer needed after this, use it as the output
                    math::Multiply<dsp::complex_t>::process(count, rtoc.out.writeBuf, pilotPLL.out.writeBuf, pilotPLL.out.writeBuf);
                    convert::ComplexToReal::process(count, pilotPLL.out.writeBuf, rdsout);
                    volk_32f_s32f_multiply_32f(rdsout, rdsout, 100.0, count);
                    rdsOutCount = rdsResamp.process(count, rdsout, rdsout);
                }

                // Filter if needed
                if (_lowPass) {
                    alFir.process(count, demod.out.writeBuf, demod.out.writeBuf);
                }

                // Interleave raw MPX to stereo
                convert::LRToStereo::process(count, demod.out.writeBuf, demod.out.writeBuf, out);
            }

            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int rdsOutCount = 0;
            process(count, base_type::_in->readBuf, base_type::out.writeBuf, rdsOutCount, rdsOut.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            if (rdsOutCount && _rdsOut) {
                if (!rdsOut.swap(rdsOutCount)) { return -1; }
            }
            return count;
        }

        stream<float> rdsOut;

    protected:
        double _deviation;
        double _samplerate;
        bool _stereo;
        bool _lowPass;
        bool _rdsOut;

        Quadrature demod;
        tap<complex_t> pilotFirTaps;
        filter::FIR<complex_t, complex_t> pilotFir;
        convert::RealToComplex rtoc;
        loop::PLL pilotPLL;
        math::Delay<float> lprDelay;
        math::Delay<complex_t> lmrDelay;
        tap<float> audioFirTaps;
        filter::FIR<float, float> arFir;
        filter::FIR<float, float> alFir;
        multirate::RationalResampler<float> rdsResamp;

        float* lmr;
        float* l;
        float* r;
        
    };
}