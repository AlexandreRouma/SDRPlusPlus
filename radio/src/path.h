#pragma once
#include <dsp/resampling.h>
#include <dsp/demodulator.h>
#include <dsp/filter.h>
#include <dsp/window.h>
#include <dsp/audio.h>
#include <module.h>
#include <signal_path/signal_path.h>
#include <dsp/processing.h>
#include <signal_path/sink.h>

class SigPath {
public:
    SigPath();
    void init(std::string vfoName);
    void start();
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


    dsp::BFMDeemp deemp;
    VFOManager::VFO* vfo;

private:
    static void sampleRateChangeHandler(float _sampleRate, void* ctx);

    dsp::stream<dsp::complex_t> input;

    // Demodulators
    dsp::FMDemod demod;
    dsp::AMDemod amDemod;
    dsp::SSBDemod ssbDemod;

    // Gain control
    dsp::AGC agc;

    // Audio output
    dsp::filter_window::BlackmanWindow audioWin;
    dsp::PolyphaseResampler<float> audioResamp;
    dsp::MonoToStereo m2s;
    SinkManager::Stream stream;

    std::string vfoName;

    // TODO: FIx all this sample rate BS (multiple names for same thing)
    float bandwidth;
    float demodOutputSamplerate;
    float outputSampleRate;
    int _demod;
    int _deemp;
    float audioBw;
};