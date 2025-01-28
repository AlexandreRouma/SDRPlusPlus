#include "vor_decoder.h"

#define STDDEV_NORM_FACTOR  1.813799364234218f // 2.0f * FL_M_PI / sqrt(12)

namespace vor {
    Decoder::Decoder(dsp::stream<dsp::complex_t>* in, double integrationTime) {
        rx.init(in);
        reshape.init(&rx.out, round(1000.0 * integrationTime), 0);
        symSink.init(&reshape.out, dataHandler, this);
    }

    Decoder::~Decoder() {
        // TODO
    }

    void Decoder::setInput(dsp::stream<dsp::complex_t>* in) {
        rx.setInput(in);
    }

    void Decoder::start() {
        rx.start();
        reshape.start();
        symSink.start();
    }

    void Decoder::stop() {
        rx.stop();
        reshape.stop();
        symSink.stop();
    }

    void Decoder::dataHandler(float* data, int count, void* ctx) {
        // Get the instance from context
        Decoder* _this = (Decoder*)ctx;

        // Compute the mean and standard deviation of the 
        float mean, stddev;
        volk_32f_stddev_and_mean_32f_x2(&stddev, &mean, data, count);
        
        // Compute the signal quality
        float quality = std::max<float>(1.0f - (stddev / STDDEV_NORM_FACTOR), 0.0f);

        // Convert the phase difference to a compass heading
        mean = -mean;
        if (mean < 0) { mean = 2.0f*FL_M_PI + mean; }

        // Call the handler
        _this->onBearing(mean, quality);
    }
}