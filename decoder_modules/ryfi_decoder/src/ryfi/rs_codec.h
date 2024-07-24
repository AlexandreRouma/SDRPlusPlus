#pragma once
#include <stdint.h>
#include "dsp/processor.h"

extern "C" {
    #include "correct.h"
}

namespace ryfi {
    // Size of an encoded reed-solomon block.
    inline const int RS_BLOCK_ENC_SIZE  = 255;

    // Size of a decoded reed-solomon block.
    inline const int RS_BLOCK_DEC_SIZE  = 223;

    // Number of reed-solomon blocks.
    inline const int RS_BLOCK_COUNT     = 4;

    // Scrambler sequence
    extern const uint8_t RS_SCRAMBLER_SEQ[RS_BLOCK_ENC_SIZE*RS_BLOCK_COUNT];

    /**
     * RyFi Reed-Solomon Encoder.
    */
    class RSEncoder : public dsp::Processor<uint8_t, uint8_t> {
        using base_type = dsp::Processor<uint8_t, uint8_t>;
    public:
        /**
         * Create a reed-solomon encoder specifying an input stream.
         * @param in Input stream
        */
        RSEncoder(dsp::stream<uint8_t>* in = NULL);

        // Destructor
        ~RSEncoder();

        /**
         * Encode data.
         * @param in Input bytes.
         * @param out Output bytes.
         * @param count Number of input bytes.
         * @return Number of output bytes.
        */
        int encode(const uint8_t* in, uint8_t* out, int count);

    private:
        int run();

        correct_reed_solomon* rs;
    };

    /**
     * RyFi Reed-Solomon Decoder.
    */
    class RSDecoder : public dsp::Processor<uint8_t, uint8_t> {
        using base_type = dsp::Processor<uint8_t, uint8_t>;
    public:
        /**
         * Create a reed-solomon decoder specifying an input stream.
         * @param in Input stream
        */
        RSDecoder(dsp::stream<uint8_t>* in = NULL);

        // Destructor
        ~RSDecoder();

        /**
         * Decode data.
         * @param in Input bytes.
         * @param out Output bytes.
         * @param count Number of input bytes.
         * @return Number of output bytes.
        */
        int decode(uint8_t* in, uint8_t* out, int count);

    private:
        int run();

        correct_reed_solomon* rs;
    };
}