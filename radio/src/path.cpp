#include <path.h>
#include <signal_path/audio.h>

SigPath::SigPath() {
    
}

int SigPath::sampleRateChangeHandler(void* ctx, double sampleRate) {
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

void SigPath::init(std::string vfoName, uint64_t sampleRate, int blockSize) {
    this->sampleRate = sampleRate;
    this->blockSize = blockSize;
    this->vfoName = vfoName;

    vfo = sigpath::vfoManager.createVFO(vfoName, ImGui::WaterfallVFO::REF_CENTER, 0, 200000, 200000, 1000);

    _demod = DEMOD_FM;
    _deemp = DEEMP_50US;
    bandwidth = 200000;
    demodOutputSamplerate = 200000;

    // TODO: Set default VFO options
    // TODO: ajust deemphasis for different output sample rates
    // TODO: Add a mono to stereo for different modes

    squelch.init(vfo->output, 800);
    squelch.level = 40;
    squelch.onCount = 1;
    squelch.offCount = 2560;

    demod.init(squelch.out[0], 100000, 200000, 800);
    amDemod.init(squelch.out[0], 50);
    ssbDemod.init(squelch.out[0], 6000, 3000, 22);
    cpx2stereo.init(squelch.out[0], 22);
    
    audioResamp.init(&demod.output, 200000, 48000, 800);
    deemp.init(&audioResamp.output, 800, 50e-6, 48000);
    
    outputSampleRate = audio::registerMonoStream(&deemp.output, vfoName, vfoName, sampleRateChangeHandler, this);
    audio::setBlockSize(vfoName, audioResamp.getOutputBlockSize());

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
    else if (_demod == DEMOD_RAW) {
        cpx2stereo.stop();
    }
    else {
        spdlog::error("UNIMPLEMENTED DEMODULATOR IN SigPath::setDemodulator (stop)");
    }
    _demod = demId;
    
    squelch.stop();

    // Set input of the audio resampler
    // TODO: Set bandwidth from argument
    if (demId == DEMOD_FM) {
        demodOutputSamplerate = 200000;
        vfo->setSampleRate(200000, bandwidth);
        demod.setBlockSize(vfo->getOutputBlockSize());
        demod.setSampleRate(200000);
        demod.setDeviation(bandwidth / 2.0f);
        audioResamp.setInput(&demod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(200000, vfo->getOutputBlockSize(), audioBw, audioBw);
        deemp.bypass = (_deemp == DEEMP_NONE);
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        demod.start();
    }
    else if (demId == DEMOD_NFM) {
        demodOutputSamplerate = 16000;
        vfo->setSampleRate(16000, bandwidth);
        demod.setBlockSize(vfo->getOutputBlockSize());
        demod.setSampleRate(16000);
        demod.setDeviation(bandwidth / 2.0f);
        audioResamp.setInput(&demod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(16000, vfo->getOutputBlockSize(), audioBw, audioBw);
        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        demod.start();
    }
    else if (demId == DEMOD_AM) {
        demodOutputSamplerate = 125000;
        vfo->setSampleRate(12500, bandwidth);
        amDemod.setBlockSize(vfo->getOutputBlockSize());
        audioResamp.setInput(&amDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(12500, vfo->getOutputBlockSize(), audioBw, audioBw);
        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        amDemod.start();
    }
    else if (demId == DEMOD_USB) {
        demodOutputSamplerate = 6000;
        vfo->setSampleRate(6000, bandwidth);
        ssbDemod.setBlockSize(vfo->getOutputBlockSize());
        ssbDemod.setMode(dsp::SSBDemod::MODE_USB);
        audioResamp.setInput(&ssbDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, vfo->getOutputBlockSize(), audioBw, audioBw);
        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_LOWER);
        ssbDemod.start();
    }
    else if (demId == DEMOD_LSB) {
        demodOutputSamplerate = 6000;
        vfo->setSampleRate(6000, bandwidth);
        ssbDemod.setBlockSize(vfo->getOutputBlockSize());
        ssbDemod.setMode(dsp::SSBDemod::MODE_LSB);
        audioResamp.setInput(&ssbDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, vfo->getOutputBlockSize(), audioBw, audioBw);
        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_UPPER);
        ssbDemod.start();
    }
    else if (demId == DEMOD_DSB) {
        demodOutputSamplerate = 6000;
        vfo->setSampleRate(6000, bandwidth);
        ssbDemod.setBlockSize(vfo->getOutputBlockSize());
        ssbDemod.setMode(dsp::SSBDemod::MODE_DSB);
        audioResamp.setInput(&ssbDemod.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(6000, vfo->getOutputBlockSize(), audioBw, audioBw);
        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        ssbDemod.start();
    }
    else if (demId == DEMOD_RAW) {
        demodOutputSamplerate = 10000;
        vfo->setSampleRate(10000, bandwidth);
        cpx2stereo.setBlockSize(vfo->getOutputBlockSize());
        //audioResamp.setInput(&cpx2stereo.output);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        audioResamp.setInputSampleRate(10000, vfo->getOutputBlockSize(), audioBw, audioBw);
        vfo->setReference(ImGui::WaterfallVFO::REF_LOWER);
        cpx2stereo.start();
    }
    else {
        spdlog::error("UNIMPLEMENTED DEMODULATOR IN SigPath::setDemodulator (start): {0}", demId);
    }

    squelch.setBlockSize(vfo->getOutputBlockSize());
    squelch.start();

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
    vfo->setBandwidth(bandwidth);
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
    else if (_demod == DEMOD_RAW) {
        // Notbing to change
    }
    else {
        spdlog::error("UNIMPLEMENTED DEMODULATOR IN SigPath::setBandwidth");
    }
    float _audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
    if (audioBw != _audioBw) {
        audioBw = _audioBw;
        audioResamp.stop();
        audioResamp.setFilterParams(audioBw, audioBw);
        audioResamp.setBlockSize(vfo->getOutputBlockSize());
        //audioResamp.setInputSampleRate(demodOutputSamplerate, vfo->getOutputBlockSize(), audioBw, audioBw);
        audioResamp.start();
    }
}

void SigPath::start() {
    squelch.start();
    demod.start();
    audioResamp.start();
    deemp.start();
    audio::startStream(vfoName);
}