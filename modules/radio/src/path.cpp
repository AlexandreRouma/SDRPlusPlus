#include <path.h>

SigPath::SigPath() {
    
}

int SigPath::sampleRateChangeHandler(void* ctx, float sampleRate) {
    SigPath* _this = (SigPath*)ctx;
    _this->outputSampleRate = sampleRate;
    _this->audioResamp.stop();
    _this->deemp.stop();
    float bw = std::min<float>(_this->bandwidth, sampleRate / 2.0f);
    spdlog::warn("New bandwidth: {0}", bw);
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
    bandwidth = 200000;

    // TODO: Set default VFO options

    demod.init(input, 100000, 200000, 800);
    amDemod.init(input, 50);
    ssbDemod.init(input, 6000, 3000, 22);
    
    audioResamp.init(&demod.output, 200000, 48000, 800);
    deemp.init(&audioResamp.output, 800, 50e-6, 48000);
    outputSampleRate = API->registerMonoStream(&deemp.output, vfoName, vfoName, sampleRateChangeHandler, this);
    API->setBlockSize(vfoName, audioResamp.getOutputBlockSize());

    setDemodulator(_demod);
}

void SigPath::setSampleRate(float sampleRate) {
    this->sampleRate = sampleRate;

    // Reset the demodulator and audio systems
    setDemodulator(_demod);
}

void SigPath::setDemodulator(int demId) {
    if (demId < 0 || demId >= _DEMOD_COUNT) {
        return;
    }

    audioResamp.stop();
    deemp.stop();

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
    _demod = demId;

    // Set input of the audio resampler
    if (demId == DEMOD_FM) {
        API->setVFOSampleRate(vfoName, 200000, 200000);
        bandwidth = 15000;
        demod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        demod.setSampleRate(200000);
        demod.setDeviation(100000);
        audioResamp.setInput(&demod.output);
        float audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(200000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = false;
        demod.start();
    }
    if (demId == DEMOD_NFM) {
        API->setVFOSampleRate(vfoName, 16000, 16000);
        bandwidth = 8000;
        demod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        demod.setSampleRate(16000);
        demod.setDeviation(8000);
        audioResamp.setInput(&demod.output);
        float audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(16000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        demod.start();
    }
    else if (demId == DEMOD_AM) {
        API->setVFOSampleRate(vfoName, 12500, 12500);
        bandwidth = 6250;
        amDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        audioResamp.setInput(&amDemod.output);
        float audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(12500, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        amDemod.start();
    }
    else if (demId == DEMOD_USB) {
        API->setVFOSampleRate(vfoName, 6000, 3000);
        bandwidth = 3000;
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_USB);
        audioResamp.setInput(&ssbDemod.output);
        float audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        ssbDemod.start();
    }
    else if (demId == DEMOD_LSB) {
        API->setVFOSampleRate(vfoName, 6000, 3000);
        bandwidth = 3000;
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_LSB);
        audioResamp.setInput(&ssbDemod.output);
        float audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        ssbDemod.start();
    }
    else if (demId == DEMOD_DSB) {
        API->setVFOSampleRate(vfoName, 6000, 6000);
        bandwidth = 3000;
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_DSB);
        audioResamp.setInput(&ssbDemod.output);
        float audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName), audioBw, audioBw);
        deemp.bypass = true;
        ssbDemod.start();
    }

    deemp.setBlockSize(audioResamp.getOutputBlockSize());

    audioResamp.start();
    deemp.start();
}

void SigPath::updateBlockSize() {
    setDemodulator(_demod);
}

void SigPath::start() {
    demod.start();
    audioResamp.start();
    deemp.start();
    API->startStream(vfoName);
}