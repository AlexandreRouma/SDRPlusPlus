#include <vfo.h>
#include <numeric>

VFO::VFO() {

}

void VFO::init(cdsp::stream<cdsp::complex_t>* input, float offset, float inputSampleRate, float bandWidth, int bufferSize) {
    _input = input;
    outputSampleRate = ceilf(bandWidth / OUTPUT_SR_ROUND) * OUTPUT_SR_ROUND;
    _inputSampleRate = inputSampleRate;
    int _gcd = std::gcd((int)inputSampleRate, (int)outputSampleRate);
    _interp = outputSampleRate / _gcd;
    _decim = inputSampleRate / _gcd;
    _bandWidth = bandWidth;
    _bufferSize = bufferSize;
    lo.init(offset, inputSampleRate, bufferSize);
    mixer.init(&lo.output, input, bufferSize);
    interp.init(&mixer.output, _interp, bufferSize);

    BlackmanWindow(decimTaps, inputSampleRate * _interp, bandWidth / 2.0f, bandWidth / 2.0f);

    if (_interp != 1) {
        printf("Interpolation needed\n");
        decFir.init(&interp.output, decimTaps, bufferSize * _interp, _decim);
    }
    else {
        decFir.init(&mixer.output, decimTaps, bufferSize, _decim);
        printf("Interpolation NOT needed: %d %d %d\n", bufferSize / _decim, _decim, _interp);
    }

    output = &decFir.output;
}


void VFO::start() {
    lo.start();
    mixer.start();
    if (_interp != 1) {
        interp.start();
    }
    decFir.start();
}

void VFO::stop() {
    // TODO: Stop LO
    mixer.stop();
    interp.stop();
    decFir.stop();
}


void VFO::setOffset(float freq) {
    lo.setFrequency(-freq);
}

void VFO::setBandwidth(float bandWidth) {
    if (bandWidth == _bandWidth) {
        return;
    }
    outputSampleRate = ceilf(bandWidth / OUTPUT_SR_ROUND) * OUTPUT_SR_ROUND;
    int _gcd = std::gcd((int)_inputSampleRate, (int)outputSampleRate);
    int interpol = outputSampleRate / _gcd;
    int decim = _inputSampleRate / _gcd;
    _bandWidth = bandWidth;

    BlackmanWindow(decimTaps, _inputSampleRate * _interp, bandWidth / 2, bandWidth);

    decFir.stop();
    decFir.setTaps(decimTaps);
    decFir.setDecimation(decim);

    if (interpol != _interp) {
        interp.stop();
        if (interpol == 1) {
            decFir.setBufferSize(_bufferSize);
            decFir.setInput(&mixer.output);
        }
        else if (_interp == 1) {
            decFir.setInput(&interp.output);
            decFir.setBufferSize(_bufferSize * _interp);
            interp.setInterpolation(interpol);
            interp.start();
        }
        else {
            decFir.setBufferSize(_bufferSize * _interp);
            interp.setInterpolation(interpol);
            interp.start();
        }
    }

    _interp = interpol;
    _decim = decim;

    decFir.start();
}

void VFO::setSampleRate(int sampleRate) {

}

int VFO::getOutputSampleRate() {
    return outputSampleRate;
}