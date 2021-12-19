#include <signal_path/dsp.h>
#include <core.h>

SignalPath::SignalPath() {
}

void SignalPath::init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*, int, void*), void* fftHandlerCtx) {
    this->sampleRate = sampleRate;
    this->sourceSampleRate = sampleRate;
    this->fftRate = fftRate;
    this->fftSize = fftSize;
    inputBlockSize = sampleRate / 200.0f;

    halfBandWindow.init(1000000, 200000, 4000000);

    inputBuffer.init(input);
    corrector.init(&inputBuffer.out, 50.0f / sampleRate);
    split.init(&inputBuffer.out);

    // Allocate the fft taps
    fftTaps = new float[fftSize];

    // Calculate the parameters for the reshaper
    int fftInterval = sampleRate / fftRate;
    fftOutputSampleCount = std::min<int>(fftInterval, fftSize);
    int fftSkip = fftInterval - fftOutputSampleCount;

    // Generate FFT Windows
    generateFFTWindow(fftWindow, fftTaps, fftOutputSampleCount);

    reshape.init(&fftStream, fftSize, fftSkip);
    split.bindStream(&fftStream);
    fftHandlerSink.init(&reshape.out, fftHandler, fftHandlerCtx);
}

void SignalPath::setSampleRate(double sampleRate) {
    this->sampleRate = sampleRate;

    // Stop the splitter
    split.stop();
    reshape.stop();

    // Stop all VFOs
    for (auto const& [name, vfo] : vfos) {
        vfo.vfo->stop();
    }

    updateFFTDSP();

    // Update the sample rate for all VFOs and start them up
    for (auto const& [name, vfo] : vfos) {
        vfo.vfo->setInSampleRate(sampleRate);
        vfo.vfo->start();
    }

    // Update correction rate on the IQ corrector
    corrector.setCorrectionRate(50.0f / sampleRate);

    // Start the splitter
    split.start();
    reshape.start();
}

double SignalPath::getSampleRate() {
    return sampleRate;
}

void SignalPath::start() {
    for (auto& decimator : decimators) {
        decimator->start();
    }
    inputBuffer.start();
    if (iqCorrection) { corrector.start(); }
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
    if (iqCorrection) { corrector.stop(); }
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
    reshape.stop();
    updateFFTDSP();
    reshape.start();
}

void SignalPath::setFFTRate(double rate) {
    fftRate = rate;
    reshape.stop();
    updateFFTDSP();
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
        if (iqCorrection) {
            split.setInput(&corrector.out);
        }
        else {
            split.setInput(&inputBuffer.out);
        }

        if (running) { split.start(); }
        core::setInputSampleRate(sourceSampleRate);
        return;
    }

    // Create new decimators
    for (int i = 0; i < dec; i++) {
        dsp::HalfDecimator<dsp::complex_t>* decimator;
        if (iqCorrection && i == 0) {
            decimator = new dsp::HalfDecimator<dsp::complex_t>(&corrector.out, &halfBandWindow);
        }
        else if (i == 0) {
            decimator = new dsp::HalfDecimator<dsp::complex_t>(&inputBuffer.out, &halfBandWindow);
        }
        else {
            decimator = new dsp::HalfDecimator<dsp::complex_t>(&decimators[i - 1]->out, &halfBandWindow);
        }

        if (running) { decimator->start(); }
        decimators.push_back(decimator);
    }
    split.setInput(&decimators[decimators.size() - 1]->out);
    if (running) { split.start(); }

    // Update the DSP sample rate
    core::setInputSampleRate(sourceSampleRate);
}

void SignalPath::setIQCorrection(bool enabled) {
    if (iqCorrection == enabled) { return; }

    if (!iqCorrection && enabled) {
        if (decimation) {
            decimators[0]->setInput(&corrector.out);
        }
        else {
            split.setInput(&corrector.out);
        }
        if (running) { corrector.start(); }
    }
    else if (iqCorrection && !enabled) {
        if (running) { corrector.stop(); }
        if (decimation) {
            decimators[0]->setInput(&inputBuffer.out);
        }
        else {
            split.setInput(&inputBuffer.out);
        }
    }

    iqCorrection = enabled;
    if (!enabled) {
        corrector.offset.re = 0;
        corrector.offset.im = 0;
    }
}

void SignalPath::setFFTWindow(int win) {
    fftWindow = win;
    reshape.stop();
    updateFFTDSP();
    reshape.start();
}

void SignalPath::generateFFTWindow(int win, float* taps, int size) {
    if (win == FFT_WINDOW_RECTANGULAR) {
        for (int i = 0; i < size; i++) {
            taps[i] = (i % 2) ? 1 : -1;
        }
    }
    else if (win == FFT_WINDOW_BLACKMAN) {
        for (int i = 0; i < size; i++) {
            taps[i] = ((i % 2) ? dsp::window_function::blackman(i, size) : -dsp::window_function::blackman(i, size)) * 2;
        }
    }
}

void SignalPath::updateFFTDSP() {

    // Allocate the fft taps
    if (fftTaps != NULL) { delete[] fftTaps; }
    fftTaps = new float[fftSize];

    // Calculate the parameters for the reshaper
    int fftInterval = sampleRate / fftRate;
    fftOutputSampleCount = std::min<int>(fftInterval, fftSize);
    int fftSkip = fftInterval - fftOutputSampleCount;

    // Generate FFT Windows
    generateFFTWindow(fftWindow, fftTaps, fftOutputSampleCount);

    // Update parameters of the reshaper
    reshape.setKeep(fftOutputSampleCount);
    reshape.setSkip(fftSkip);

    spdlog::info("Updating FFT DSP settings: Keep: {0}, Skip: {1}", fftOutputSampleCount, fftSkip);
}