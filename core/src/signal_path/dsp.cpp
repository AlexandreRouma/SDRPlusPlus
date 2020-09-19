#include <signal_path/dsp.h>

SignalPath::SignalPath() {
    
}

void SignalPath::init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*)) {
    this->sampleRate = sampleRate;
    this->fftRate = fftRate;
    this->fftSize = fftSize;
    inputBlockSize = sampleRate / 200.0f;

    dcBiasRemover.init(input, 32000);
    dcBiasRemover.bypass = true;
    split.init(&dcBiasRemover.output, 32000);

    fftBlockDec.init(&split.output_a, (sampleRate / fftRate) - fftSize, fftSize);
    fftHandlerSink.init(&fftBlockDec.output, fftBuffer, fftSize, fftHandler);

    dynSplit.init(&split.output_b, 32000);
}

void SignalPath::setSampleRate(float sampleRate) {
    this->sampleRate = sampleRate;
    inputBlockSize = sampleRate / 200.0f;

    dcBiasRemover.stop();
    split.stop();
    fftBlockDec.stop();
    fftHandlerSink.stop();
    dynSplit.stop();

    for (auto const& [name, vfo] : vfos) {
        vfo.vfo->stop();
    }

    dcBiasRemover.setBlockSize(inputBlockSize);
    split.setBlockSize(inputBlockSize);
    int skip = (sampleRate / fftRate) - fftSize;
    fftBlockDec.setSkip(skip);
    dynSplit.setBlockSize(inputBlockSize);

    mod::broadcastEvent(mod::EVENT_STREAM_PARAM_CHANGED);

    for (auto const& [name, vfo] : vfos) {
        vfo.vfo->setInputSampleRate(sampleRate, inputBlockSize);
        vfo.vfo->start();
    }

    fftHandlerSink.start();
    fftBlockDec.start();
    split.start();
    dcBiasRemover.start();
    dynSplit.start();
}

void SignalPath::start() {
    dcBiasRemover.start();
    split.start();

    fftBlockDec.start();
    fftHandlerSink.start();

    dynSplit.start();
}

void SignalPath::setDCBiasCorrection(bool enabled) {
    dcBiasRemover.bypass = !enabled;
}

dsp::VFO* SignalPath::addVFO(std::string name, float outSampleRate, float bandwidth, float offset) {
    if (vfos.find(name) != vfos.end()) {
        return NULL;
    }
    dynSplit.stop();
    VFO_t vfo;
    vfo.inputStream = new dsp::stream<dsp::complex_t>(inputBlockSize * 2);
    dynSplit.bind(vfo.inputStream);
    vfo.vfo = new dsp::VFO();
    vfo.vfo->init(vfo.inputStream, sampleRate, outSampleRate, bandwidth, offset, inputBlockSize);
    vfo.vfo->start();
    vfos[name] = vfo;
    dynSplit.start();
    return vfo.vfo;
}

void SignalPath::removeVFO(std::string name) {
    if (vfos.find(name) == vfos.end()) {
        return;
    }
    dynSplit.stop();
    VFO_t vfo = vfos[name];
    vfo.vfo->stop();
    dynSplit.unbind(vfo.inputStream);
    delete vfo.vfo;
    delete vfo.inputStream;
    dynSplit.start();
    vfos.erase(name);
}