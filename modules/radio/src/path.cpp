#include <path.h>

SigPath::SigPath() {
    
}

void SigPath::init(std::string vfoName, uint64_t sampleRate, int blockSize, dsp::stream<dsp::complex_t>* input) {
    this->sampleRate = sampleRate;
    this->blockSize = blockSize;
    this->vfoName = vfoName;

    _demod = DEMOD_FM;

    // TODO: Set default VFO options

    demod.init(input, 100000, 200000, 800);
    amDemod.init(input, 50);
    ssbDemod.init(input, 6000, 3000, 22);
    
    audioResamp.init(&demod.output, 200000, 48000, 800);
    audio.init(&audioResamp.output, 64);
}

void SigPath::setSampleRate(float sampleRate) {
    this->sampleRate = sampleRate;

    // Reset the demodulator and audio systems
    setDemodulator(_demod);
}

void SigPath::setVolume(float volume) {
    audio.setVolume(volume);
}

void SigPath::setDemodulator(int demId) {
    if (demId < 0 || demId >= _DEMOD_COUNT) {
        return;
    }

    audioResamp.stop();

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
        demod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        demod.setSampleRate(200000);
        demod.setDeviation(100000);
        audioResamp.setInput(&demod.output);
        audioResamp.setInputSampleRate(200000, API->getVFOOutputBlockSize(vfoName));
        demod.start();
    }
    if (demId == DEMOD_NFM) {
        API->setVFOSampleRate(vfoName, 12500, 12500);
        demod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        demod.setSampleRate(12500);
        demod.setDeviation(6250);
        audioResamp.setInput(&demod.output);
        audioResamp.setInputSampleRate(12500, API->getVFOOutputBlockSize(vfoName));
        demod.start();
    }
    else if (demId == DEMOD_AM) {
        API->setVFOSampleRate(vfoName, 12500, 12500);
        amDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        audioResamp.setInput(&amDemod.output);
        audioResamp.setInputSampleRate(12500, API->getVFOOutputBlockSize(vfoName));
        amDemod.start();
    }
    else if (demId == DEMOD_USB) {
        API->setVFOSampleRate(vfoName, 6000, 3000);
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_USB);
        audioResamp.setInput(&ssbDemod.output);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName));
        ssbDemod.start();
    }
    else if (demId == DEMOD_LSB) {
        API->setVFOSampleRate(vfoName, 6000, 3000);
        ssbDemod.setBlockSize(API->getVFOOutputBlockSize(vfoName));
        ssbDemod.setMode(dsp::SSBDemod::MODE_LSB);
        audioResamp.setInput(&ssbDemod.output);
        audioResamp.setInputSampleRate(6000, API->getVFOOutputBlockSize(vfoName));
        ssbDemod.start();
    }

    audioResamp.start();
}

void SigPath::updateBlockSize() {
    setDemodulator(_demod);
}

void SigPath::start() {
    demod.start();
    audioResamp.start();
    audio.start();
}

void SigPath::DEBUG_TEST() {
    audio.stop();
    audio.start();
}