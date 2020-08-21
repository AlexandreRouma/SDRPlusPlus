#include <path.h>

SigPath::SigPath() {
    
}

int SigPath::sampleRateChangeHandler(void* ctx, float sampleRate) {
    SigPath* _this = (SigPath*)ctx;
    _this->outputSampleRate = sampleRate;
    _this->audioResamp.stop();
    _this->deemp.stop();
    float bw = std::min<float>(_this->bandwidth, sampleRate / 2.0f);
    _this->audioResamp.setOutputSampleRate(sampleRate, bw, bw);
    _this->deemp.setBlockSize(_this->audioResamp.getOutputBlockSize());
    _this->deemp.setSamplerate(sampleRate);
    _this->audioResamp.start();
    _this->deemp.start();
    return _this->audioResamp.getOutputBlockSize();
}

void SigPath::init(std::string vfoName, uint64_t sampleRate, int blockSize, dsp::stream<dsp::complex_t>* input) {
    this->sampleRate = sampleRate;
    this->blockSize = blockSize;
    this->vfoName = vfoName;

    _demod = DEMOD_FM;
    _deemp = DEEMP_50US;
    bandwidth = 200000;

    // TODO: Set default VFO options

    demod.init(input, 100000, 200000, 800);
    amDemod.init(input, 50);
    ssbDemod.init(input, 6000, 3000, 22);
    
    audioResamp.init(&demod.output, 200000, 48000, 800);
    deemp.init(&audioResamp.output, 800, 50e-6, 48000);
    outputSampleRate = API->registerMonoStream(&deemp.output, vfoName, vfoName, sampleRateChangeHandler, this);
    API->setBlockSize(vfoName, audioResamp.getOutputBlockSize());

    setDemodulator(_demod, bandwidth);
}

void SigPath::setSampleRate(float sampleRate) {
    this->sampleRate = sampleRate;

    // Reset the demodulator and audio systems
    setDemodulator(_demod, bandwidth);
}

void SigPath::setDemodulator(int demId, float bandWidth) {
    if (demId < 0 || demId >= _DEMOD_COUNT) {
        return;
    }

    audioResamp.stop();
    deemp.stop();

    bandwidth = bandWidth;

    // Stop current demodulator
    if (_demod == DEMOD_FM) {
        demod.stop();
    }
    else if (_demod == DEMOD_NFM) {
        demod.stop();
    }
    else if (_demod == DEMOD_AM) {
        amDemod.stop();
    }
    else if (_demod == DEMOD_USB) {
        ssbDemod.stop();
    }
    else if (_demod == DEMOD_LSB) {
        ssbDemod.stop();
    }
    else if (_demod == DEMOD_DSB) {
        ssbDemod.stop();
    }
    else {
        spdlog::error("UNIMPLEMENTED DEMODULATOR IN SigPath::setDemodulator (stop)");
    }
    _demod = demId;

    // Set input of the audio resampler
    // TODO: Set bandwidth from argument
    if (demId == DEMOD_FM) {
        API->setVFOSampleRate(vfoName, 200000, bandwidth);
        demod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        demod.setSampleRate(200000);
        demod.setDeviation(bandwidth / 2.0f);
        audioResamp.setInput(&demod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(200000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = (_deemp == DEEMP_NONE);
        demod.start();
    }
    if (demId == DEMOD_NFM) {
        API->setVFOSampleRate(vfoName, 16000, bandwidth);
        demod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        demod.setSampleRate(16000);
        demod.setDeviation(bandwidth / 2.0f);
        audioResamp.setInput(&demod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(16000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        demod.start();
    }
    else if (demId == DEMOD_AM) {
        API->setVFOSampleRate(vfoName, 12500, bandwidth);
        amDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        audioResamp.setInput(&amDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(12500, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        amDemod.start();
    }
    else if (demId == DEMOD_USB) {
        API->setVFOSampleRate(vfoName, 6000, bandwidth);
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_USB);
        audioResamp.setInput(&ssbDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        ssbDemod.start();
    }
    else if (demId == DEMOD_LSB) {
        API->setVFOSampleRate(vfoName, 6000, bandwidth);
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_LSB);
        audioResamp.setInput(&ssbDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        ssbDemod.start();
    }
    else if (demId == DEMOD_DSB) {
        API->setVFOSampleRate(vfoName, 6000, bandwidth);
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_DSB);
        audioResamp.setInput(&ssbDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        ssbDemod.start();
    }
    else {
        spdlog::error("UNIMPLEMENTED DEMODULATOR IN SigPath::setDemodulator (start)");
    }

    deemp.setBlockSize(audioResamp.getOutputBlockSize());

    audioResamp.start();
    deemp.start();
}

void SigPath::updateBlockSize() {
    setDemodulator(_demod, bandwidth);
}

void SigPath::setDeemphasis(int deemph) {
    _deemp = deemph;
    deemp.stop();
    if (_deemp == DEEMP_NONE) {
        deemp.bypass = true;
    }
    else if (_deemp == DEEMP_50US) {
        deemp.bypass = false;
        deemp.setTau(50e-6);
    }
    else if (_deemp == DEEMP_75US) {
        deemp.bypass = false;
        deemp.setTau(75e-6);
    }
    deemp.start();
}

void SigPath::setBandwidth(float bandWidth) {
    bandwidth = bandWidth;
    API->setVFOBandwidth(vfoName, bandwidth);
    if (_demod == DEMOD_FM) {
        demod.stop();
        demod.setDeviation(bandwidth / 2.0f);
        demod.start();
    }
    else if (_demod == DEMOD_NFM) {
        demod.stop();
        demod.setDeviation(bandwidth / 2.0f);
        demod.start();
    }
    else if (_demod == DEMOD_AM) {
        // Notbing to change
    }
    else if (_demod == DEMOD_USB) {
        ssbDemod.stop();
        ssbDemod.setBandwidth(bandwidth);
        ssbDemod.start();
    }
    else if (_demod == DEMOD_LSB) {
        ssbDemod.stop();
        ssbDemod.setBandwidth(bandwidth);
        ssbDemod.start();
    }
    else if (_demod == DEMOD_DSB) {
        ssbDemod.stop();
        ssbDemod.setBandwidth(bandwidth);
        ssbDemod.start();
    }
    else {
        spdlog::error("UNIMPLEMENTED DEMODULATOR IN SigPath::setBandwidth");
    }
    float _audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
    if (audioBw != _audioBw) {
        audioBw = _audioBw;
        audioResamp.stop();
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        audioResamp.start();
    }
}

void SigPath::start() {
    demod.start();
    audioResamp.start();
    deemp.start();
    API->startStream(vfoName);
}