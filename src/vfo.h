#pragma once
#include <cdsp/math.h>
#include <cdsp/generator.h>
#include <cdsp/resampling.h>
#include <cdsp/filter.h>

// Round up to next 5KHz multiple frequency
#define OUTPUT_SR_ROUND 5000.0f

class VFO {
public:
    VFO();
    void init(cdsp::stream<cdsp::complex_t>* input, float offset, float sampleRate, float bandWidth, int bufferSize);

    void start();
    void stop();

    void setOffset(float freq);
    void setBandwidth(float bandwidth);
    void setSampleRate(int sampleRate);

    int getOutputSampleRate();

    cdsp::stream<cdsp::complex_t>* output;

private:
    cdsp::ComplexSineSource lo;
    cdsp::Multiplier mixer;
    cdsp::IQInterpolator interp;
    cdsp::DecimatingFIRFilter decFir;

    std::vector<float> decimTaps;

    int _interp;
    int _decim;
    float _inputSampleRate;
    float _outputSampleRate;
    float _bandWidth;
    int _bufferSize;
    int outputSampleRate;
    
    cdsp::stream<cdsp::complex_t>* _input;
};