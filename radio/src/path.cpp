#include <path.h>
#include <signal_path/audio.h>
#include <spdlog/spdlog.h>

SigPath::SigPath() {
    
}

int SigPath::sampleRateChangeHandler(void* ctx, double sampleRate) {
    SigPath* _this = (SigPath*)ctx;
    _this->outputSampleRate = sampleRate;
    _this->audioResamp.stop();
    _this->deemp.stop();
    float bw = std::min<float>(_this->bandwidth, sampleRate / 2.0f);

    
    _this->audioResamp.setOutSampleRate(sampleRate);
    _this->audioWin.setSampleRate(_this->sampleRate * _this->audioResamp.getInterpolation());
    _this->audioResamp.updateWindow(&_this->audioWin);

    _this->deemp.setSampleRate(sampleRate);
    _this->audioResamp.start();
    _this->deemp.start();
    // Note: returning a block size should not be needed anymore
    return 1;
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

    demod.init(vfo->output, 200000, 100000);
    amDemod.init(vfo->output);
    ssbDemod.init(vfo->output, 6000, 3000, dsp::SSBDemod::MODE_USB);

    audioWin.init(24000, 24000, 200000);
    audioResamp.init(&demod.out, &audioWin, 200000, 48000);
    audioWin.setSampleRate(audioResamp.getInterpolation() * 200000);
    audioResamp.updateWindow(&audioWin);

    deemp.init(&audioResamp.out, 48000, 50e-6);
    
    outputSampleRate = audio::registerMonoStream(&deemp.out, vfoName, vfoName, sampleRateChangeHandler, this);

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
        demodOutputSamplerate = 200000;
        vfo->setSampleRate(200000, bandwidth);
        demod.setSampleRate(200000);
        demod.setDeviation(bandwidth / 2.0f);
        audioResamp.setInput(&demod.out);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);

        audioResamp.setInSampleRate(200000);
        audioWin.setSampleRate(200000 * audioResamp.getInterpolation());
        audioWin.setCutoff(audioBw);
        audioWin.setTransWidth(audioBw);
        audioResamp.updateWindow(&audioWin);

        deemp.bypass = (_deemp == DEEMP_NONE);
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        demod.start();
    }
    else if (demId == DEMOD_NFM) {
        demodOutputSamplerate = 16000;
        vfo->setSampleRate(16000, bandwidth);
        demod.setSampleRate(16000);
        demod.setDeviation(bandwidth / 2.0f);
        audioResamp.setInput(&demod.out);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        
        audioResamp.setInSampleRate(16000);
        audioWin.setSampleRate(16000 * audioResamp.getInterpolation());
        audioWin.setCutoff(audioBw);
        audioWin.setTransWidth(audioBw);
        audioResamp.updateWindow(&audioWin);

        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        demod.start();
    }
    else if (demId == DEMOD_AM) {
        demodOutputSamplerate = 125000;
        vfo->setSampleRate(12500, bandwidth);
        audioResamp.setInput(&amDemod.out);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        
        audioResamp.setInSampleRate(12500);
        audioWin.setSampleRate(12500 * audioResamp.getInterpolation());
        audioWin.setCutoff(audioBw);
        audioWin.setTransWidth(audioBw);
        audioResamp.updateWindow(&audioWin);

        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        amDemod.start();
    }
    else if (demId == DEMOD_USB) {
        demodOutputSamplerate = 6000;
        vfo->setSampleRate(6000, bandwidth);
        ssbDemod.setMode(dsp::SSBDemod::MODE_USB);
        audioResamp.setInput(&ssbDemod.out);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        
        audioResamp.setInSampleRate(6000);
        audioWin.setSampleRate(6000 * audioResamp.getInterpolation());
        audioWin.setCutoff(audioBw);
        audioWin.setTransWidth(audioBw);
        audioResamp.updateWindow(&audioWin);

        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_LOWER);
        ssbDemod.start();
    }
    else if (demId == DEMOD_LSB) {
        demodOutputSamplerate = 6000;
        vfo->setSampleRate(6000, bandwidth);
        ssbDemod.setMode(dsp::SSBDemod::MODE_LSB);
        audioResamp.setInput(&ssbDemod.out);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        
        audioResamp.setInSampleRate(6000);
        audioWin.setSampleRate(6000 * audioResamp.getInterpolation());
        audioWin.setCutoff(audioBw);
        audioWin.setTransWidth(audioBw);
        audioResamp.updateWindow(&audioWin);

        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_UPPER);
        ssbDemod.start();
    }
    else if (demId == DEMOD_DSB) {
        demodOutputSamplerate = 6000;
        vfo->setSampleRate(6000, bandwidth);
        ssbDemod.setMode(dsp::SSBDemod::MODE_DSB);
        audioResamp.setInput(&ssbDemod.out);
        audioBw = std::min<float>(bandwidth, outputSampleRate / 2.0f);
        
        audioResamp.setInSampleRate(6000);
        audioWin.setSampleRate(6000 * audioResamp.getInterpolation());
        audioWin.setCutoff(audioBw);
        audioWin.setTransWidth(audioBw);
        audioResamp.updateWindow(&audioWin);

        deemp.bypass = true;
        vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        ssbDemod.start();
    }
    else {
        spdlog::error("UNIMPLEMENTED DEMODULATOR IN SigPath::setDemodulator (start): {0}", demId);
    }

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
        ssbDemod.setBandWidth(bandwidth);
        ssbDemod.start();
    }
    else if (_demod == DEMOD_LSB) {
        ssbDemod.stop();
        ssbDemod.setBandWidth(bandwidth);
        ssbDemod.start();
    }
    else if (_demod == DEMOD_DSB) {
        ssbDemod.stop();
        ssbDemod.setBandWidth(bandwidth);
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

        audioWin.setCutoff(audioBw);
        audioWin.setTransWidth(audioBw);
        audioResamp.updateWindow(&audioWin);

        audioResamp.start();
    }
}

void SigPath::start() {
    demod.start();
    audioResamp.start();
    deemp.start();
    //ns.start();
    audio::startStream(vfoName);
}