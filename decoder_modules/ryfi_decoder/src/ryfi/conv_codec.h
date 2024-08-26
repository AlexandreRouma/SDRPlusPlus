#pragma once
#include <stdint.h>
#include <stddef.h>
#include "dsp/processor.h"

extern "C" {
    #include "correct.h"
}

namespace ryfi {
    /**
     * RyFi Convolutional Encoder.
    */
    class ConvEncoder : public dsp::Processor<uint8_t, uint8_t> {
        using base_type = dsp::Processor<uint8_t, uint8_t>;
    public:
        /**
         * Create a convolutional encoder specifying an input stream.
         * @param in Input stream.
        */
        ConvEncoder(dsp::stream<uint8_t>* in = NULL);

        // Destructor
        ~ConvEncoder();

        /**
         * Encode data.
         * @param in Input bytes.
         * @param out Output bits.
         * @param count Number of input bytes.
         * @return Number of output bits.
        */
        int encode(const uint8_t* in, uint8_t* out, int count);

    private:
        int run();

        correct_convolutional* conv;
    };

    /**
     * RyFi Convolutional Decoder.
    */
    class ConvDecoder : public dsp::Processor<dsp::complex_t, uint8_t> {
        using base_type = dsp::Processor<dsp::complex_t, uint8_t>;
    public:
        /**
         * Create a convolutional encoder specifying an input stream.
         * @param in Input stream.
        */
        ConvDecoder(dsp::stream<dsp::complex_t>* in = NULL);

        // Destructor
        ~ConvDecoder();

        /**
         * Decode soft symbols.
         * @param in Input soft symbols.
         * @param out Output bytes.
         * @param count Number of input bytes.
         * @return Number of output bits.
        */
        int decode(const dsp::complex_t* in, uint8_t* out, int count);

    private:
        int run();

        correct_convolutional* conv;
        uint8_t* soft = NULL;
    };
}