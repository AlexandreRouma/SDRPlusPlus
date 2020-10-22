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
#include <map>
#include <module.h>

class SignalPath {
public:
    SignalPath();
    void init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*));
    void start();
    void setSampleRate(double sampleRate);
    void setDCBiasCorrection(bool enabled);
    void setFFTRate(double rate);
    double getSampleRate();
    dsp::VFO* addVFO(std::string name, double outSampleRate, double bandwidth, double offset);
    void removeVFO(std::string name);
    void setInput(dsp::stream<dsp::complex_t>* input);
    void bindIQStream(dsp::stream<dsp::complex_t>* stream);
    void unbindIQStream(dsp::stream<dsp::complex_t>* stream);

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

    double sampleRate;
    double fftRate;
    int fftSize;
    int inputBlockSize;
};