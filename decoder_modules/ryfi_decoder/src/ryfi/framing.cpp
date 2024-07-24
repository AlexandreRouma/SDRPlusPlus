#include "framing.h"

namespace ryfi {
    dsp::complex_t QPSK_SYMBOLS[4] = {
        { -0.070710678118f, -0.070710678118f },
        { -0.070710678118f,  0.070710678118f },
        {  0.070710678118f, -0.070710678118f },
        {  0.070710678118f,  0.070710678118f },
    };

    Framer::Framer(dsp::stream<uint8_t>* in) {
        // Generate the sync symbols
        int k = 0;
        for (int i = 62; i >= 0; i -= 2) {
            syncSyms[k++] = QPSK_SYMBOLS[(SYNC_WORD >> i) & 0b11];
        }
        

        // Initialize base class
        base_type::init(in);
    }

    int Framer::encode(const uint8_t* in, dsp::complex_t* out, int count) {
        // Copy sync symbols
        memcpy(out, syncSyms, SYNC_SYMS*sizeof(dsp::complex_t));

        // Modulate the rest of the bits
        dsp::complex_t* dataOut = &out[SYNC_SYMS];
        int dataSyms = count / 2;
        for (int i = 0; i < dataSyms; i++) {
            uint8_t bits = (in[i >> 2] >> (6 - 2*(i & 0b11))) & 0b11;
            dataOut[i] = QPSK_SYMBOLS[bits];
        }

        // Compute and return the total number of symbols
        return SYNC_SYMS + dataSyms;
    }

    int Framer::run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        count = encode(base_type::_in->readBuf, base_type::out.writeBuf, count);

        base_type::_in->flush();
        if (!out.swap(count)) { return -1; }
        return count;
    }

    Deframer::Deframer(dsp::stream<dsp::complex_t> *in) {
        // Compute sync word rotations
        //   0: 00 01 11 10
        //  90: 10 00 01 11
        // 180: 11 10 00 01
        // 270: 01 11 10 00

        // For 0 and 180 it's the sync and its complement
        syncRots[ROT_0_DEG] = SYNC_WORD;
        syncRots[ROT_180_DEG] = ~SYNC_WORD;
        
        // For 90 and 270 its the quadrature and its complement
        uint64_t quad;
        for (int i = 62; i >= 0; i -= 2) {
            // Get the symbol
            uint8_t sym = (SYNC_WORD >> i) & 0b11;

            // Rotate it 90 degrees
            uint8_t rsym;
            switch (sym) {
                case 0b00:  rsym = 0b10; break;
                case 0b01:  rsym = 0b00; break;
                case 0b11:  rsym = 0b01; break;
                case 0b10:  rsym = 0b11; break;
            }

            // Push it into the quadrature
            quad = (quad << 2) | rsym;
        }
        syncRots[ROT_90_DEG] = quad;
        syncRots[ROT_270_DEG] = ~quad;

        base_type::init(in);
    }

    int Deframer::run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        dsp::complex_t* in = base_type::_in->readBuf;

        for (int i = 0; i < count; i++) {
            if (recv) {
                // Copy the symbol to the output and rotate it approprieate
                base_type::out.writeBuf[outCount++] = in[i] * symRot;

                // Check if we're done receiving the frame, send it out
                if (!(--recv)) {
                    if (!base_type::out.swap(outCount)) {
                        base_type::_in->flush();
                        return -1;
                    }
                }
            }
            else {
                // Get the raw symbol
                dsp::complex_t fsym = in[i];

                // Decode the symbol
                uint8_t sym = ((fsym.re > 0) ? 0b10 : 0b00) | ((fsym.im > 0) ? 0b01 : 0b00);

                // Push it to the shift register
                shift = (shift << 2) | sym;

                // Find the rotation starting with the last known one
                for (int i = 0; i < 4; i++) {
                    // Get the test rotation
                    int testRot = (knownRot+i) & 0b11;

                    // Check if the hamming distance is close enough
                    int dist;
                    if (distance(shift, syncRots[testRot]) < 6) {
                        // Save the new rotation
                        knownRot = testRot;

                        // Start reading in symbols for the frame
                        symRot = symRots[knownRot];
                        recv = 8168; // TODO: Don't hardcode!
                        outCount = 0;
                    }
                }
            }
        }

        base_type::_in->flush();
        return count;
    }
}