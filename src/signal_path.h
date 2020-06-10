#pragma once
#include <cdsp/filter.h>
#include <cdsp/resampling.h>
#include <cdsp/generator.h>
#include <cdsp/math.h>

class SignalPath {
public:
    SignalPath();
    SignalPath(cdsp::stream<cdsp::complex_t>* source, float sampleRate, float fftRate, int fftSize, cdsp::complex_t* fftBuffer, void fftHandler(cdsp::complex_t*));
    void start();
    void setSampleRate(float sampleRate);
    void setDCBiasCorrection(bool enabled);
    void setFFTRate(float rate);

private:
    cdsp::DCBiasRemover dcBiasRemover;
    cdsp::BlockDecimator fftBlockDec;
    cdsp::ComplexSineSource fftSineSource;
    cdsp::Multiplier fftMul;
    cdsp::HandlerSink fftHandler;

    float sampleRate;
    float fftRate;
    int fftSize;
};