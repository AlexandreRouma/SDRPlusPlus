#pragma once
#include <dsp/block.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/routing.h>
#include <dsp/demodulator.h>
#include <dsp/sink.h>
#include <spdlog/spdlog.h>

extern "C" {
#include <correct.h>
}

#define KGSSTV_DEVIATION        300
#define KGSSTV_BAUDRATE         1200
#define KGSSTV_RRC_ALPHA        0.7f
#define KGSSTV_4FSK_HIGH_CUT    0.5f

// const uint8_t KGSSTV_SYNC_WORD[] = {
//     0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0,
//     0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0,
//     0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0,
//     0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0,
//     1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
//     0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1,
//     0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
//     1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0
// };

const uint8_t KGSSTV_SYNC_WORD[] = {
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0,
    0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1,
    0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0
};

const uint8_t KGSSTV_SCRAMBLING[] = {
    1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0,
    1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0,
    0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1,
    1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0,
    0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1
};

const uint8_t KGSSTV_SCRAMBLING_BYTES[] = {
    0b11101100, 0b11000100, 0b10011100, 0b11111001, 0b00000100, 
    0b01101010, 0b10011011, 0b01001010, 0b00010110, 0b00011001, 
    0b01111111, 0b01011011, 0b10111100, 0b01110100, 0b01010111,
    0b00000010
};

static const correct_convolutional_polynomial_t kgsstv_polynomial[] = {0155, 0117};

#define KGSSTV_SYNC_WORD_SIZE       sizeof(KGSSTV_SYNC_WORD)
#define KGSSTV_SYNC_SCRAMBLING_SIZE sizeof(KGSSTV_SCRAMBLING)

namespace kgsstv {
    // class Slice4FSK : public dsp::generic_block<Slice4FSK> {
    // public:
    //     Slice4FSK() {}

    //     Slice4FSK(dsp::stream<float>* in) { init(in); }

    //     void init(dsp::stream<float>* in) {
    //         _in = in;
    //         dsp::generic_block<Slice4FSK>::registerInput(_in);
    //         dsp::generic_block<Slice4FSK>::registerOutput(&out);
    //         dsp::generic_block<Slice4FSK>::_block_init = true;
    //     }

    //     void setInput(dsp::stream<float>* in) {
    //         assert(dsp::generic_block<Slice4FSK>::_block_init);
    //         std::lock_guard<std::mutex> lck(dsp::generic_block<Slice4FSK>::ctrlMtx);
    //         dsp::generic_block<Slice4FSK>::tempStop();
    //         dsp::generic_block<Slice4FSK>::unregisterInput(_in);
    //         _in = in;
    //         dsp::generic_block<Slice4FSK>::registerInput(_in);
    //         dsp::generic_block<Slice4FSK>::tempStart();
    //     }

    //     int run() {
    //         int count = _in->read();
    //         if (count < 0) { return -1; }

    //         float val;
    //         for (int i = 0; i < count; i++) {
    //             val = _in->readBuf[i];

    //             out.writeBuf[i * 2] = (val > 0.0f);
    //             if (val > 0.0f) {
    //                 out.writeBuf[(i * 2) + 1] = (val > KGSSTV_4FSK_HIGH_CUT);
    //             }
    //             else {
    //                 out.writeBuf[(i * 2) + 1] = (val > -KGSSTV_4FSK_HIGH_CUT);
    //             }
    //         }

    //         _in->flush();

    //         if (!out.swap(count * 2)) { return -1; }
    //         return count;
    //     }

    //     dsp::stream<uint8_t> out;

    // private:
    //     dsp::stream<float>* _in;
    // };

    class Deframer : public dsp::generic_block<Deframer> {
    public:
        Deframer() {}

        Deframer(dsp::stream<float>* in) { init(in); }

        void init(dsp::stream<float>* in) {
            _in = in;

            // TODO: Destroy
            conv = correct_convolutional_create(2, 7, kgsstv_polynomial);
            memset(convTmp, 0x00, 1024);

            dsp::generic_block<Deframer>::registerInput(_in);
            dsp::generic_block<Deframer>::registerOutput(&out);
            dsp::generic_block<Deframer>::_block_init = true;
        }

        void setInput(dsp::stream<float>* in) {
            assert(dsp::generic_block<Deframer>::_block_init);
            std::lock_guard<std::mutex> lck(dsp::generic_block<Deframer>::ctrlMtx);
            dsp::generic_block<Deframer>::tempStop();
            dsp::generic_block<Deframer>::unregisterInput(_in);
            _in = in;
            dsp::generic_block<Deframer>::registerInput(_in);
            dsp::generic_block<Deframer>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            for (int i = 0; i < count; i++) {
                if (syncing) {
                    // If sync broken, reset sync
                    if ((_in->readBuf[i] > 0.0f) && !KGSSTV_SYNC_WORD[match]) {
                        if (++err > 4) {
                            i -= match - 1;
                            match = 0;
                            err = 0;
                            continue;
                        }
                    }

                    // If full syncword was detected, switch to read mode
                    if (++match == KGSSTV_SYNC_WORD_SIZE) {
                        spdlog::warn("Frame detected");
                        syncing = false;
                        readCount = 0;
                        writeCount = 0;
                    }
                }
                else {
                    // // Process symbol
                    // if (!(readCount % 2)) {
                    //     int bitOffset = writeCount & 0b111;
                    //     int byteOffset = writeCount >> 3;
                    //     if (!bitOffset) { convTmp[byteOffset] = 0; }
                    //     convTmp[byteOffset] |= _in->readBuf[i] << (7 - bitOffset);
                    //     writeCount++;
                    // }

                    // Process symbol
                    convTmp[readCount] = std::clamp<int>((_in->readBuf[i] + 1.0f) * 128.0f, 0, 255);
                    
                    // When info was read, write data and get back to
                    if (++readCount == 108) {
                        match = 0;
                        err = 0;
                        syncing = true;

                        // Descramble
                        for (int j = 0; j < 108; j++) {
                            if (KGSSTV_SCRAMBLING[j]) {
                                convTmp[j] = 255 - convTmp[j];
                            }
                    
                            //convTmp[j >> 3] ^= KGSSTV_SCRAMBLING[j] << (7 - (j & 0b111));
                        }

                        // Decode convolutional code
                        int convOutCount = correct_convolutional_decode_soft(conv, convTmp, 124, out.writeBuf);

                        spdlog::warn("Frames written: {0}, frameBytes: {1}", ++framesWritten, convOutCount);
                        if (!out.swap(7)) {
                            _in->flush();
                            return -1;
                        }
                    }
                }
            }

            _in->flush();

            return count;
        }

        dsp::stream<uint8_t> out;

    private:
        dsp::stream<float>* _in;
        correct_convolutional* conv = NULL;
        uint8_t convTmp[1024];

        int match = 0;
        int err = 0;
        int readCount = 0;
        int writeCount = 0;
        bool syncing = true;

        int framesWritten = 0;
    };

    class Decoder : public dsp::generic_hier_block<Decoder> {
    public:
        Decoder() {}

        Decoder(dsp::stream<dsp::complex_t>* input, float sampleRate) {
            init(input, sampleRate);
        }

        void init(dsp::stream<dsp::complex_t>* input, float sampleRate) {
            _sampleRate = sampleRate;

            demod.init(input, _sampleRate, KGSSTV_DEVIATION);
            rrc.init(31, _sampleRate, KGSSTV_BAUDRATE, KGSSTV_RRC_ALPHA);
            fir.init(&demod.out, &rrc);
            recov.init(&fir.out, _sampleRate / KGSSTV_BAUDRATE, 1e-6f, 0.01f, 0.01f);
            doubler.init(&recov.out);

            //slicer.init(&doubler.outA);
            deframer.init(&doubler.outA);
            ns2.init(&deframer.out, "kgsstv_out.bin");
            diagOut = &doubler.outB;

            dsp::generic_hier_block<Decoder>::registerBlock(&demod);
            dsp::generic_hier_block<Decoder>::registerBlock(&fir);
            dsp::generic_hier_block<Decoder>::registerBlock(&recov);
            dsp::generic_hier_block<Decoder>::registerBlock(&doubler);
            //dsp::generic_hier_block<Decoder>::registerBlock(&slicer);
            dsp::generic_hier_block<Decoder>::registerBlock(&deframer);
            dsp::generic_hier_block<Decoder>::registerBlock(&ns2);

            dsp::generic_hier_block<Decoder>::_block_init = true;
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            assert(dsp::generic_hier_block<Decoder>::_block_init);
            demod.setInput(input);
        }

        dsp::stream<float>* diagOut = NULL;

    private:
        dsp::FloatFMDemod demod;
        dsp::RRCTaps rrc;
        dsp::FIR<float> fir;
        dsp::MMClockRecovery<float> recov;
        dsp::StreamDoubler<float> doubler;

        // Slice4FSK slicer;
        Deframer deframer;
        dsp::FileSink<uint8_t> ns2;


        float _sampleRate;
    };
}