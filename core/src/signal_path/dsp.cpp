#include <signal_path/dsp.h>

SignalPath::SignalPath() {
    
}

void SignalPath::init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*,int,void*)) {
    this->sampleRate = sampleRate;
    this->fftRate = fftRate;
    this->fftSize = fftSize;
    inputBlockSize = sampleRate / 200.0f;

    split.init(input);

    reshape.init(&fftStream, fftSize, (sampleRate / fftRate) - fftSize);
    split.bindStream(&fftStream);
    fftHandlerSink.init(&reshape.out, fftHandler, NULL);
}

void SignalPath::setSampleRate(double sampleRate) {
    this->sampleRate = sampleRate;

    split.stop();

    for (auto const& [name, vfo] : vfos) {
        vfo.vfo->stop();
    }

    // Claculate skip to maintain a constant fft rate
    int skip = (sampleRate / fftRate) - fftSize;
    reshape.setSkip(skip);

    // TODO: Tell modules that the block size has changed (maybe?)

    for (auto const& [name, vfo] : vfos) {
        vfo.vfo->setInSampleRate(sampleRate);
        vfo.vfo->start();
    }

    split.start();
}

double SignalPath::getSampleRate() {
    return sampleRate;
}

void SignalPath::start() {
    split.start();
    reshape.start();
    fftHandlerSink.start();
}

void SignalPath::stop() {
    split.stop();
    reshape.stop();
    fftHandlerSink.stop();
}

dsp::VFO* SignalPath::addVFO(std::string name, double outSampleRate, double bandwidth, double offset) {
    if (vfos.find(name) != vfos.end()) {
        return NULL;
    }
    VFO_t vfo;
    vfo.inputStream = new dsp::stream<dsp::complex_t>;
    split.bindStream(vfo.inputStream);
    vfo.vfo = new dsp::VFO();
    vfo.vfo->init(vfo.inputStream, offset, sampleRate, outSampleRate, bandwidth);
    vfo.vfo->start();
    vfos[name] = vfo;
    return vfo.vfo;
}

void SignalPath::removeVFO(std::string name) {
    if (vfos.find(name) == vfos.end()) {
        return;
    }
    VFO_t vfo = vfos[name];
    vfo.vfo->stop();
    split.unbindStream(vfo.inputStream);
    delete vfo.vfo;
    delete vfo.inputStream;
    vfos.erase(name);
}

void SignalPath::setInput(dsp::stream<dsp::complex_t>* input) {
    split.setInput(input);
}

void SignalPath::bindIQStream(dsp::stream<dsp::complex_t>* stream) {
    split.bindStream(stream);
}

void SignalPath::unbindIQStream(dsp::stream<dsp::complex_t>* stream) {
    split.unbindStream(stream);
}

void SignalPath::setFFTSize(int size) {
    fftSize = size;
    int skip = (sampleRate / fftRate) - fftSize;
    reshape.setSkip(skip);
}

void SignalPath::startFFT() {
    reshape.start();
    fftHandlerSink.start();
}

void SignalPath::stopFFT() {
    reshape.stop();
    fftHandlerSink.stop();
}