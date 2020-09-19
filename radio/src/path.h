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
    void setVFOFrequency(uint64_t frequency);
    void updateBlockSize();
    void setDemodulator(int demod, float bandWidth);
    void setDeemphasis(int deemph);
    void setBandwidth(float bandWidth);

    enum {
        DEMOD_FM,
        DEMOD_NFM,
        DEMOD_AM,
        DEMOD_USB,
        DEMOD_LSB,
        DEMOD_DSB,
        DEMOD_RAW,
        _DEMOD_COUNT
    };

    enum {
        DEEMP_50US,
        DEEMP_75US,
        DEEMP_NONE,
        _DEEMP_COUNT
    };


    dsp::FMDeemphasis deemp;

private:
    static int sampleRateChangeHandler(void* ctx, float sampleRate);
    
    dsp::stream<dsp::complex_t> input;

    // Demodulators
    dsp::FMDemodulator demod;
    dsp::AMDemodulator amDemod;
    dsp::SSBDemod ssbDemod;
    dsp::ComplexToStereo cpx2stereo;

    // Audio output
    dsp::MonoToStereo m2s;
    dsp::FIRResampler<float> audioResamp;

    std::string vfoName;

    float sampleRate;
    float bandwidth;
    float outputSampleRate;
    int blockSize;
    int _demod;
    int _deemp;
    float audioBw;
};