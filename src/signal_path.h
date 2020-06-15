#pragma once
#include <cdsp/filter.h>
#include <cdsp/resampling.h>
#include <cdsp/generator.h>
#include <cdsp/math.h>
#include <cdsp/demodulation.h>
#include <cdsp/audio.h>
#include <vfo.h>

class SignalPath {
public:
    SignalPath();
    void init(uint64_t sampleRate, int fftRate, int fftSize, cdsp::stream<cdsp::complex_t>* input, cdsp::complex_t* fftBuffer, void fftHandler(cdsp::complex_t*));
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
    cdsp::DCBiasRemover dcBiasRemover;
    cdsp::Splitter split;

    // FFT
    cdsp::BlockDecimator fftBlockDec;
    cdsp::HandlerSink fftHandlerSink;

    // VFO
    VFO mainVFO;

    cdsp::FMDemodulator demod;
    cdsp::AMDemodulator amDemod;

    //cdsp::FloatDecimatingFIRFilter audioDecFilt;
    cdsp::FractionalResampler audioResamp;
    cdsp::AudioSink audio;

    float sampleRate;
    float fftRate;
    int fftSize;
    int _demod;
};