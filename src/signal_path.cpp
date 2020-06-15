#include <signal_path.h>

SignalPath::SignalPath() {
    
}

std::vector<float> iftaps;
std::vector<float> ataps;

void SignalPath::init(uint64_t sampleRate, int fftRate, int fftSize, cdsp::stream<cdsp::complex_t>* input, cdsp::complex_t* fftBuffer, void fftHandler(cdsp::complex_t*)) {
    this->sampleRate = sampleRate;
    this->fftRate = fftRate;
    this->fftSize = fftSize;

    BlackmanWindow(iftaps, sampleRate, 100000, 200000);
    BlackmanWindow(ataps, 200000, 20000, 10000);

    // for (int i = 0; i < iftaps.size(); i++) {
    //     printf("%f\n", iftaps[i]);
    // }

    _demod = DEMOD_FM;

    printf("%d\n", iftaps.size());
    printf("%d\n", ataps.size());

    dcBiasRemover.init(input, 32000);
    dcBiasRemover.bypass = true;
    split.init(&dcBiasRemover.output, 32000);

    fftBlockDec.init(&split.output_a, (sampleRate / fftRate) - fftSize, fftSize);
    fftHandlerSink.init(&fftBlockDec.output, fftBuffer, fftSize, fftHandler);

    mainVFO.init(&split.output_b, 0, sampleRate, 200000, 32000);

    demod.init(mainVFO.output, 100000, 200000, 800);
    amDemod.init(mainVFO.output, 800);
    
    audioResamp.init(&demod.output, 200000, 40000, 800, 20000);
    audio.init(audioResamp.output, 160);
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
        mainVFO.setBandwidth(200000);
        audioResamp.setInput(&demod.output);
        audioResamp.setInputSampleRate(200000);
        demod.start();
    }
    else if (demId == DEMOD_AM) {
        printf("Starting AM demodulator\n");
        mainVFO.setBandwidth(12000);
        audioResamp.setInput(&amDemod.output);
        audioResamp.setInputSampleRate(12000);
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
    
    audioResamp.start();
    audio.start();
}