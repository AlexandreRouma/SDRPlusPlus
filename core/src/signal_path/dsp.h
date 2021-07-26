#pragma once
#include <dsp/routing.h>
#include <dsp/vfo.h>
#include <map>
#include <dsp/sink.h>
#include <dsp/decimation.h>

class SignalPath {
public:
    SignalPath();
    void init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*,int,void*), void* fftHandlerCtx);
    void start();
    void stop();
    void setSampleRate(double sampleRate);
    double getSampleRate();
    dsp::VFO* addVFO(std::string name, double outSampleRate, double bandwidth, double offset);
    void removeVFO(std::string name);
    void setInput(dsp::stream<dsp::complex_t>* input);
    void bindIQStream(dsp::stream<dsp::complex_t>* stream);
    void unbindIQStream(dsp::stream<dsp::complex_t>* stream);
    void setFFTSize(int size);
    void startFFT();
    void stopFFT();
    void setBuffering(bool enabled);
    void setDecimation(int dec);

    dsp::SampleFrameBuffer<dsp::complex_t> inputBuffer;
    double sourceSampleRate = 0;
    int decimation = 0;

private:
    struct VFO_t {
        dsp::stream<dsp::complex_t>* inputStream;
        dsp::VFO* vfo;
    };

    dsp::Splitter<dsp::complex_t> split;

    // FFT
    dsp::stream<dsp::complex_t> fftStream;
    dsp::Reshaper<dsp::complex_t> reshape;
    dsp::HandlerSink<dsp::complex_t> fftHandlerSink;

    // VFO
    std::map<std::string, VFO_t> vfos;
    std::vector<dsp::HalfDecimator<dsp::complex_t>*> decimators;
    dsp::filter_window::BlackmanWindow halfBandWindow;

    double sampleRate;
    double fftRate;
    int fftSize;
    int inputBlockSize;
    bool bufferingEnabled = false;
    bool running = false;
};