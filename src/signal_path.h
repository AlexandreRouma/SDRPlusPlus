#pragma once
#include <dsp/filter.h>
#include <dsp/resampling.h>
#include <dsp/source.h>
#include <dsp/math.h>
#include <dsp/demodulator.h>
#include <dsp/routing.h>
#include <dsp/sink.h>
#include <dsp/correction.h>
#include <dsp/vfo.h>
#include <io/audio.h>

class SignalPath {
public:
    SignalPath();
    void init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*));
    void start();
    void setSampleRate(float sampleRate);
    void setDCBiasCorrection(bool enabled);
    void setFFTRate(float rate);

    void setVFOFrequency(long frequency);
    void setVolume(float volume);

    void setDemodulator(int demod);

    enum {
        DEMOD_FM,
        DEMOD_AM,
        _DEMOD_COUNT
    };

private:
    dsp::DCBiasRemover dcBiasRemover;
    dsp::Splitter split;

    // FFT
    dsp::BlockDecimator fftBlockDec;
    dsp::HandlerSink fftHandlerSink;

    // VFO
    dsp::VFO mainVFO;

    // Demodulators
    dsp::FMDemodulator demod;
    dsp::AMDemodulator amDemod;

    // Audio output
    dsp::FloatResampler audioResamp;
    io::AudioSink audio;

    // DEBUG
    dsp::NullSink ns;

    float sampleRate;
    float fftRate;
    int fftSize;
    int _demod;
};