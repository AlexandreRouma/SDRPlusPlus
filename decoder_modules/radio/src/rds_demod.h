#pragma once
#include <dsp/processor.h>
#include <dsp/loop/fast_agc.h>
#include <dsp/loop/costas.h>
#include <dsp/taps/band_pass.h>
#include <dsp/filter/fir.h>
#include <dsp/convert/complex_to_real.h>
#include <dsp/clock_recovery/mm.h>
#include <dsp/digital/binary_slicer.h>
#include <dsp/digital/differential_decoder.h>

class RDSDemod : public dsp::Processor<dsp::complex_t, uint8_t> {
    using base_type = dsp::Processor<dsp::complex_t, uint8_t>;
public:
    RDSDemod() {}
    RDSDemod(dsp::stream<dsp::complex_t>* in, bool enableSoft) { init(in, enableSoft); }
    ~RDSDemod() {}

    void init(dsp::stream<dsp::complex_t>* in, bool enableSoft) {
        // Save config
        this->enableSoft = enableSoft;

        // Initialize the DSP
        agc.init(NULL, 1.0, 1e6, 0.1);
        costas.init(NULL, 0.005f);
        taps = dsp::taps::bandPass<dsp::complex_t>(0, 2375, 100, 5000);
        fir.init(NULL, taps);
        double baudfreq = dsp::math::hzToRads(2375.0/2.0, 5000);
        costas2.init(NULL, 0.01, 0.0, baudfreq, baudfreq - (baudfreq*0.1), baudfreq + (baudfreq*0.1));
        recov.init(NULL, 5000.0 / (2375.0 / 2.0), 1e-6, 0.01, 0.01);
        diff.init(NULL, 2);

        // Free useless buffers
        agc.out.free();
        fir.out.free();
        costas2.out.free();
        recov.out.free();

        // Init the rest
        base_type::init(in);
    }

    void setSoftEnabled(bool enable) {
        assert(base_type::_block_init);
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        base_type::tempStop();
        enableSoft = enable;
        base_type::tempStart();
    }

    void reset() {
        assert(base_type::_block_init);
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        base_type::tempStop();
        agc.reset();
        costas.reset();
        fir.reset();
        costas2.reset();
        recov.reset();
        diff.reset();
        base_type::tempStart();
    }

    inline int process(int count, dsp::complex_t* in, float* softOut, uint8_t* hardOut) {
        count = agc.process(count, in, costas.out.readBuf);
        count = costas.process(count, costas.out.readBuf, costas.out.writeBuf);
        count = fir.process(count, costas.out.writeBuf, costas.out.writeBuf);
        count = costas2.process(count, costas.out.writeBuf, costas.out.readBuf);
        count = dsp::convert::ComplexToReal::process(count, costas.out.readBuf, softOut);
        count = recov.process(count, softOut, softOut);
        count = dsp::digital::BinarySlicer::process(count, softOut, diff.out.readBuf);
        count = diff.process(count, diff.out.readBuf, hardOut);
        return count;
    }

    int run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        count = process(count, base_type::_in->readBuf, soft.writeBuf, base_type::out.writeBuf);

        base_type::_in->flush();
        if (!base_type::out.swap(count)) { return -1; }
        if (enableSoft) {
            if (!soft.swap(count)) { return -1; }
        }
        return count;
    }

    dsp::stream<float> soft;

private:
    bool enableSoft = false;
    
    dsp::loop::FastAGC<dsp::complex_t> agc;
    dsp::loop::Costas<2> costas;
    dsp::tap<dsp::complex_t> taps;
    dsp::filter::FIR<dsp::complex_t, dsp::complex_t> fir;
    dsp::loop::Costas<2> costas2;
    dsp::clock_recovery::MM<float> recov;
    dsp::digital::DifferentialDecoder diff;
};