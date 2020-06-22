#include <signal_path.h>

SignalPath::SignalPath() {
    
}

void SignalPath::init(uint64_t sampleRate, int fftRate, int fftSize, dsp::stream<dsp::complex_t>* input, dsp::complex_t* fftBuffer, void fftHandler(dsp::complex_t*)) {
    this->sampleRate = sampleRate;
    this->fftRate = fftRate;
    this->fftSize = fftSize;

    // for (int i = 0; i < iftaps.size(); i++) {
    //     printf("%f\n", iftaps[i]);
    // }

    _demod = DEMOD_FM;

    dcBiasRemover.init(input, 32000);
    dcBiasRemover.bypass = true;
    split.init(&dcBiasRemover.output, 32000);

    fftBlockDec.init(&split.output_a, (sampleRate / fftRate) - fftSize, fftSize);
    fftHandlerSink.init(&fftBlockDec.output, fftBuffer, fftSize, fftHandler);

    mainVFO.init(&split.output_b, sampleRate, 200000, 200000, 0, 32000);

    demod.init(mainVFO.output, 100000, 200000, 800);
    amDemod.init(mainVFO.output, 50);
    
    audioResamp.init(&demod.output, 200000, 40000, 20000, 800);
    audio.init(audioResamp.output, 160);

    ns.init(mainVFO.output, 800);
}

void SignalPath::setVFOFrequency(long frequency) {
    mainVFO.setOffset(frequency);
}

void SignalPath::setVolume(float volume) {
    audio.setVolume(volume);
}

void SignalPath::setDemodulator(int demId) {
    if (demId < 0 || demId >= _DEMOD_COUNT) {
        return;
    }

    audioResamp.stop();

    // Stop current demodulator
    if (_demod == DEMOD_FM) {
        printf("Stopping FM demodulator\n");
        demod.stop();
    }
    else if (_demod == DEMOD_AM) {
        printf("Stopping AM demodulator\n");
        amDemod.stop();
    }
    _demod = demId;

    // Set input of the audio resampler
    if (demId == DEMOD_FM) {
        printf("Starting FM demodulator\n");
        mainVFO.setOutputSampleRate(200000, 200000);
        audioResamp.setInput(&demod.output);
        audioResamp.setInputSampleRate(200000, 800);
        demod.start();
    }
    else if (demId == DEMOD_AM) {
        printf("Starting AM demodulator\n");
        mainVFO.setOutputSampleRate(12500, 12500);
        audioResamp.setInput(&amDemod.output);
        audioResamp.setInputSampleRate(12500, 50);
        amDemod.start();
    }

    audioResamp.start();
}

void SignalPath::start() {
    dcBiasRemover.start();
    split.start();

    fftBlockDec.start();
    fftHandlerSink.start();

    mainVFO.start();
    demod.start();
    //ns.start();
    
    audioResamp.start();
    audio.start();
}