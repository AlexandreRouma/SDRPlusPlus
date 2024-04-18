#pragma once
#include <dsp/stream.h>
#include <dsp/buffer/reshaper.h>
#include <dsp/multirate/rational_resampler.h>
#include <dsp/sink/handler_sink.h>
#include <dsp/demod/quadrature.h>
#include <dsp/clock_recovery/mm.h>
#include <dsp/taps/root_raised_cosine.h>
#include <dsp/correction/dc_blocker.h>
#include <dsp/loop/fast_agc.h>
#include <dsp/digital/binary_slicer.h>
#include <dsp/routing/doubler.h>

class POCSAGDSP : public dsp::Processor<dsp::complex_t, uint8_t> {
    using base_type = dsp::Processor<dsp::complex_t, uint8_t>;
public:
    POCSAGDSP() {}
    POCSAGDSP(dsp::stream<dsp::complex_t>* in, double samplerate, double baudrate) { init(in, samplerate, baudrate); }

    void init(dsp::stream<dsp::complex_t>* in, double samplerate, double baudrate) {
        // Save settings
        _samplerate = samplerate;

        // Configure blocks
        demod.init(NULL, -4500.0, samplerate);
        float taps[] = { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f };
        shape = dsp::taps::fromArray<float>(10, taps);
        fir.init(NULL, shape);
        recov.init(NULL, samplerate/baudrate, 1e-4, 1.0, 0.05);

        // Free useless buffers
        fir.out.free();
        recov.out.free();

        // Init base
        base_type::init(in);
    }

    int process(int count, dsp::complex_t* in, float* softOut, uint8_t* out) {
        count = demod.process(count, in, demod.out.readBuf);
        count = fir.process(count, demod.out.readBuf, demod.out.readBuf);
        count = recov.process(count, demod.out.readBuf, softOut);
        dsp::digital::BinarySlicer::process(count, softOut, out);
        return count;
    }

    void setBaudrate(double baudrate) {
        assert(base_type::_block_init);
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        base_type::tempStop();
        
        base_type::tempStart();
    }

    int run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        count = process(count, base_type::_in->readBuf, soft.writeBuf, base_type::out.writeBuf);

        base_type::_in->flush();
        if (!base_type::out.swap(count)) { return -1; }
        if (count) { if (!soft.swap(count)) { return -1; } }
        return count;
    }

    dsp::stream<float> soft;

private:
    dsp::demod::Quadrature demod;
    dsp::tap<float> shape;
    dsp::filter::FIR<float, float> fir;
    dsp::clock_recovery::MM<float> recov;

    double _samplerate;
};