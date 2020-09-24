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
#include <map>
#include <module.h>

class SignalPath {
public:
    SignalPath();
    void init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*));
    void start();
    void setSampleRate(float sampleRate);
    void setDCBiasCorrection(bool enabled);
    void setFFTRate(float rate);
    dsp::VFO* addVFO(std::string name, float outSampleRate, float bandwidth, float offset);
    void removeVFO(std::string name);

private:
    struct VFO_t {
        dsp::stream<dsp::complex_t>* inputStream;
        dsp::VFO* vfo;
    };

    dsp::DCBiasRemover dcBiasRemover;
    dsp::Splitter split;

    // FFT
    dsp::BlockDecimator fftBlockDec;
    dsp::HandlerSink fftHandlerSink;

    // VFO
    dsp::DynamicSplitter<dsp::complex_t> dynSplit;
    std::map<std::string, VFO_t> vfos;

    float sampleRate;
    float fftRate;
    int fftSize;
    int inputBlockSize;
};