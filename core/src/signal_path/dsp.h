#pragma once
#include <dsp/routing.h>
#include <dsp/vfo.h>
#include <map>
#include <dsp/sink.h>
#include <dsp/decimation.h>
#include <dsp/correction.h>

enum {
    FFT_WINDOW_RECTANGULAR,
    FFT_WINDOW_BLACKMAN,
    _FFT_WINDOW_COUNT
};

class SignalPath {
public:
    SignalPath();
    void init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*, int, void*), void* fftHandlerCtx);
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
    void setFFTRate(double rate);
    void startFFT();
    void stopFFT();
    void setBuffering(bool enabled);
    void setDecimation(int dec);
    void setIQCorrection(bool enabled);
    void setFFTWindow(int win);

    dsp::SampleFrameBuffer<dsp::complex_t> inputBuffer;
    double sourceSampleRate = 0;
    int decimation = 0;

    float* fftTaps = NULL;

private:
    void generateFFTWindow(int win, float* taps, int size);
    void updateFFTDSP();

    struct VFO_t {
        dsp::stream<dsp::complex_t>* inputStream;
        dsp::VFO* vfo;
    };

    dsp::Splitter<dsp::complex_t> split;
    dsp::IQCorrector corrector;

    // FFT
    dsp::stream<dsp::complex_t> fftStream;
    dsp::Reshaper<dsp::complex_t> reshape;
    dsp::HandlerSink<dsp::complex_t> fftHandlerSink;

    // VFO
    std::map<std::string, VFO_t> vfos;
    std::vector<dsp::HalfDecimator<dsp::complex_t>*> decimators;
    dsp::filter_window::BlackmanWindow halfBandWindow;

    int fftOutputSampleCount = 0;
    double sampleRate;
    double fftRate;
    int fftSize;
    int inputBlockSize;
    int fftWindow = FFT_WINDOW_RECTANGULAR;
    bool bufferingEnabled = false;
    bool running = false;
    bool iqCorrection = false;
};