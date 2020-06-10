#include <signal_path.h>

SignalPath::SignalPath(cdsp::stream<cdsp::complex_t>* source, float sampleRate, float fftRate, int fftSize, cdsp::complex_t* fftBuffer, void fftHandler(cdsp::complex_t*)) : 
    dcBiasRemover(source, 64000), 
    fftBlockDec(&dcBiasRemover.output, (sampleRate / fftRate) - fftSize, fftSize),
    fftSineSource(sampleRate / 2.0f, sampleRate, fftSize),
    fftMul(&fftBlockDec.output, &fftSineSource.output, fftSize),
    fftHandler(&fftMul.output, fftBuffer, fftSize, fftHandler) {
    this->sampleRate = sampleRate;
    this->fftRate = fftRate;
    this->fftSize = fftSize;
}

void SignalPath::start() {
    dcBiasRemover.start();
    fftBlockDec.start();
    fftSineSource.start();
    fftMul.start();
    fftHandler.start();
}