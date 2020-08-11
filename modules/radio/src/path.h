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
#include <module.h>

class SigPath {
public:
    SigPath();
    void init(std::string vfoName, uint64_t sampleRate, int blockSize, dsp::stream<dsp::complex_t>* input);
    void start();
    void setSampleRate(float sampleRate);

    void setVFOFrequency(long frequency);
    void setVolume(float volume);

    void updateBlockSize();

    void setDemodulator(int demod);

    enum {
        DEMOD_FM,
        DEMOD_NFM,
        DEMOD_AM,
        DEMOD_USB,
        DEMOD_LSB,
        _DEMOD_COUNT
    };

private:
    dsp::stream<dsp::complex_t> input;

    // Demodulators
    dsp::FMDemodulator demod;
    dsp::AMDemodulator amDemod;
    dsp::SSBDemod ssbDemod;

    // Audio output
    dsp::FloatFIRResampler audioResamp;
    io::AudioSink audio;

    std::string vfoName;

    float sampleRate;
    int blockSize;
    int _demod;
};