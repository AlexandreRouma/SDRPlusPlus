#include <signal_path/dsp.h>
#include <core.h>

SignalPath::SignalPath() {
    
}

void SignalPath::init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*,int,void*), void* fftHandlerCtx) {
    this->sampleRate = sampleRate;
    this->sourceSampleRate = sampleRate;
    this->fftRate = fftRate;
    this->fftSize = fftSize;
    inputBlockSize = sampleRate / 200.0f;

    halfBandWindow.init(1000000, 200000, 4000000);

    // split.init(input);
    inputBuffer.init(input);
    split.init(&inputBuffer.out);

    reshape.init(&fftStream, fftSize, (sampleRate / fftRate) - fftSize);
    split.bindStream(&fftStream);
    fftHandlerSink.init(&reshape.out, fftHandler, fftHandlerCtx);
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
    for (auto& decimator : decimators) {
        decimator->start();
    }
    inputBuffer.start();
    split.start();
    reshape.start();
    fftHandlerSink.start();
    running = true;
}

void SignalPath::stop() {
    for (auto& decimator : decimators) {
        decimator->stop();
    }
    inputBuffer.stop();
    split.stop();
    reshape.stop();
    fftHandlerSink.stop();
    running = false;
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
    inputBuffer.setInput(input);
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
    reshape.stop();
    reshape.setSkip(skip);
    reshape.setKeep(fftSize);
    reshape.start();
}

void SignalPath::startFFT() {
    reshape.start();
    fftHandlerSink.start();
}

void SignalPath::stopFFT() {
    reshape.stop();
    fftHandlerSink.stop();
}

void SignalPath::setBuffering(bool enabled) {
    inputBuffer.bypass = !enabled;
}

void SignalPath::setDecimation(int dec) {
    decimation = dec;
    if (running) { split.stop(); }

    // Stop existing decimators
    if (!decimators.empty()) {
        for (auto& decimator : decimators) {
            decimator->stop();
        }
        for (auto& decimator : decimators) {
            delete decimator;
        }
    }
    decimators.clear();

    // If no decimation, reconnect
    if (!dec) {
        split.setInput(&inputBuffer.out);
        if (running) { split.start(); }
        core::setInputSampleRate(sourceSampleRate);
        return;
    }
    
    // Create new decimators
    for (int i = 0; i < dec; i++) {
        dsp::HalfDecimator<dsp::complex_t>* decimator = new dsp::HalfDecimator<dsp::complex_t>((i == 0) ? &inputBuffer.out : &decimators[i-1]->out, &halfBandWindow);
        if (running) { decimator->start(); }
        // TODO: ONLY start if running
        decimators.push_back(decimator);
    }
    split.setInput(&decimators[decimators.size()-1]->out);
    if (running) { split.start(); }

    // Update the DSP sample rate
    core::setInputSampleRate(sourceSampleRate);
}