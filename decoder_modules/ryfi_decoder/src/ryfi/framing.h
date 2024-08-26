#pragma once
#include "dsp/processor.h"
#include <stdint.h>
#include <stddef.h>

namespace ryfi {
    // Synchronization word.
    inline const uint64_t SYNC_WORD = 0x341CC540819D8963;

    // Number of synchronization bits.
    inline const int SYNC_BITS      = 64;

    // Number of synchronization symbols.
    inline const int SYNC_SYMS      = SYNC_BITS / 2;

    // Possible constellation rotations
    enum {
        ROT_0_DEG       = 0,
        ROT_90_DEG      = 1,
        ROT_180_DEG     = 2,
        ROT_270_DEG     = 3
    };

    /**
     * RyFi Framer.
    */
    class Framer : public dsp::Processor<uint8_t, dsp::complex_t> {
        using base_type = dsp::Processor<uint8_t, dsp::complex_t>;
    public:
        /**
         * Create a framer specifying an input stream.
         * @param in Input stream.
        */
        Framer(dsp::stream<uint8_t>* in = NULL);

        /**
         * Encode a frame to symbols adding a sync word.
        */
        int encode(const uint8_t* in, dsp::complex_t* out, int count);

    private:
        int run();

        dsp::complex_t syncSyms[SYNC_SYMS];
    };

    class Deframer : public dsp::Processor<dsp::complex_t, dsp::complex_t> {
        using base_type = dsp::Processor<dsp::complex_t, dsp::complex_t>;
    public:
        /**
         * Create a deframer specifying an input stream.
         * @param in Input stream.
        */
        Deframer(dsp::stream<dsp::complex_t> *in = NULL);

    private:
        int run();

        inline static constexpr int distance(uint64_t a, uint64_t b) {
            int dist = 0;
            for (int i = 0; i < 64; i++) {
                dist += ((a & 1) != (b & 1));
                a >>= 1;
                b >>= 1;
            }
            return dist;
        }

        // Frame reading counters
        int recv = 0;
        int outCount = 0;

        // Rotation handling
        int knownRot = 0;
        uint64_t syncRots[4];
        dsp::complex_t symRot;
        const dsp::complex_t symRots[4] = {
            {  1.0f,  0.0f }, //   0 deg
            {  0.0f, -1.0f }, //  90 deg
            { -1.0f,  0.0f }, // 180 deg
            {  0.0f,  1.0f }, // 270 deg
        };

        // Shift register
        uint64_t shift;
    };
}