#pragma once
#include <dsp/block.h>
#include <volk/volk.h>
#include <codec2.h>
#include <dsp/demodulator.h>

extern "C" {
    #include <correct.h>
}

#define M17_DEVIATION               2400.0f
#define M17_BAUDRATE                4800.0f
#define M17_RRC_ALPHA               0.5f
#define M17_4FSK_HIGH_CUT           0.52f

#define M17_SYNC_SIZE               16
#define M17_LICH_SIZE               96
#define M17_PAYLOAD_SIZE            144
#define M17_ENCODED_PAYLOAD_SIZE    296
#define M17_RAW_FRAME_SIZE          384
#define M17_CUT_FRAME_SIZE          368

const uint8_t M17_LSF_SYNC[16] = {      0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1 };
const uint8_t M17_STF_SYNC[16] = {      1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 };
const uint8_t M17_PKF_SYNC[16] = {      0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

const uint8_t M17_SCRAMBLER[368] = {    1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1,
                                        1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0,
                                        1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0,
                                        1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0,
                                        1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0,
                                        1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0,
                                        1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
                                        0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0,
                                        0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1,
                                        1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1,
                                        1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0,
                                        0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1,
                                        0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0,
                                        0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0,
                                        1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0,
                                        0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1,
                                        1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
                                        1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1,
                                        1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,
                                        0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,
                                        0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1,
                                        0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1 };

const uint16_t M17_INTERLEAVER[368] = { 0, 137, 90, 227, 180, 317, 270, 39, 360, 129, 82, 219, 172, 309, 262, 31,
                                        352, 121, 74, 211, 164, 301, 254, 23, 344, 113, 66, 203, 156, 293, 246, 15,
                                        336, 105, 58, 195, 148, 285, 238, 7, 328, 97, 50, 187, 140, 277, 230, 367,
                                        320, 89, 42, 179, 132, 269, 222, 359, 312, 81, 34, 171, 124, 261, 214, 351,
                                        304, 73, 26, 163, 116, 253, 206, 343, 296, 65, 18, 155, 108, 245, 198, 335,
                                        288, 57, 10, 147, 100, 237, 190, 327, 280, 49, 2, 139, 92, 229, 182, 319,
                                        272, 41, 362, 131, 84, 221, 174, 311, 264, 33, 354, 123, 76, 213, 166, 303,
                                        256, 25, 346, 115, 68, 205, 158, 295, 248, 17, 338, 107, 60, 197, 150, 287,
                                        240, 9, 330, 99, 52, 189, 142, 279, 232, 1, 322, 91, 44, 181, 134, 271,
                                        224, 361, 314, 83, 36, 173, 126, 263, 216, 353, 306, 75, 28, 165, 118, 255,
                                        208, 345, 298, 67, 20, 157, 110, 247, 200, 337, 290, 59, 12, 149, 102, 239,
                                        192, 329, 282, 51, 4, 141, 94, 231, 184, 321, 274, 43, 364, 133, 86, 223,
                                        176, 313, 266, 35, 356, 125, 78, 215, 168, 305, 258, 27, 348, 117, 70, 207,
                                        160, 297, 250, 19, 340, 109, 62, 199, 152, 289, 242, 11, 332, 101, 54, 191,
                                        144, 281, 234, 3, 324, 93, 46, 183, 136, 273, 226, 363, 316, 85, 38, 175,
                                        128, 265, 218, 355, 308, 77, 30, 167, 120, 257, 210, 347, 300, 69, 22, 159,
                                        112, 249, 202, 339, 292, 61, 14, 151, 104, 241, 194, 331, 284, 53, 6, 143,
                                        96, 233, 186, 323, 276, 45, 366, 135, 88, 225, 178, 315, 268, 37, 358, 127,
                                        80, 217, 170, 307, 260, 29, 350, 119, 72, 209, 162, 299, 252, 21, 342, 111,
                                        64, 201, 154, 291, 244, 13, 334, 103, 56, 193, 146, 283, 236, 5, 326, 95,
                                        48, 185, 138, 275, 228, 365, 318, 87, 40, 177, 130, 267, 220, 357, 310, 79,
                                        32, 169, 122, 259, 212, 349, 302, 71, 24, 161, 114, 251, 204, 341, 294, 63,
                                        16, 153, 106, 243, 196, 333, 286, 55, 8, 145, 98, 235, 188, 325, 278, 47 };

const uint8_t M17_PUNCTURING_P1[61] = { 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
                                        1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
                                        1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
                                        1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1 };

const uint8_t M17_PUNCTURING_P2[12] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 };

static const correct_convolutional_polynomial_t correct_conv_m17_polynomial[] = {0b11001, 0b10111};

namespace dsp {
    class M17Slice4FSK : public generic_block<M17Slice4FSK> {
    public:
        M17Slice4FSK() {}

        M17Slice4FSK(stream<float>* in) { init(in); }

        void init(stream<float>* in) {
            _in = in;
            generic_block<M17Slice4FSK>::registerInput(_in);
            generic_block<M17Slice4FSK>::registerOutput(&out);
            generic_block<M17Slice4FSK>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<M17Slice4FSK>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<M17Slice4FSK>::ctrlMtx);
            generic_block<M17Slice4FSK>::tempStop();
            generic_block<M17Slice4FSK>::unregisterInput(_in);
            _in = in;
            generic_block<M17Slice4FSK>::registerInput(_in);
            generic_block<M17Slice4FSK>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            float val;
            for (int i = 0; i < count; i++) {
                val = _in->readBuf[i];
                out.writeBuf[i*2] = (val < 0.0f);
                out.writeBuf[(i*2)+1] = (fabsf(val) > M17_4FSK_HIGH_CUT);
            }

            _in->flush();

            if (!out.swap(count * 2)) { return -1; }
            return count;
        }

        stream<uint8_t> out;

    private:
        stream<float>* _in;

    };

    class M17FrameDemux : public generic_block<M17FrameDemux> {
    public:
        M17FrameDemux() {}

        M17FrameDemux(stream<uint8_t>* in) { init(in); }

        ~M17FrameDemux() {
            if (!generic_block<M17FrameDemux>::_block_init) { return; }
            generic_block<M17FrameDemux>::stop();
            delete[] delay;
        }

        void init(stream<uint8_t>* in) {
            _in = in;

            delay = new uint8_t[STREAM_BUFFER_SIZE];

            generic_block<M17FrameDemux>::registerInput(_in);
            generic_block<M17FrameDemux>::registerOutput(&linkSetupOut);
            generic_block<M17FrameDemux>::registerOutput(&lichOut);
            generic_block<M17FrameDemux>::registerOutput(&streamOut);
            generic_block<M17FrameDemux>::registerOutput(&packetOut);
            generic_block<M17FrameDemux>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<M17FrameDemux>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<M17FrameDemux>::ctrlMtx);
            generic_block<M17FrameDemux>::tempStop();
            generic_block<M17FrameDemux>::unregisterInput(_in);
            _in = in;
            generic_block<M17FrameDemux>::registerInput(_in);
            generic_block<M17FrameDemux>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            memcpy(&delay[M17_SYNC_SIZE], _in->readBuf, count);

            for (int i = 0; i < count;) {
                if (detect) {
                    if (outCount < M17_SYNC_SIZE) { outCount++; i++; }
                    else {
                        int id = M17_INTERLEAVER[outCount - M17_SYNC_SIZE];

                        if (type == 0) {
                            linkSetupOut.writeBuf[id] = delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE];
                        }
                        else if (type == 1 && id < M17_LICH_SIZE) {
                            lichOut.writeBuf[id/8] |= (delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE]) << (7 - (id%8));
                        }
                        else if (type == 1) {
                            streamOut.writeBuf[id - M17_LICH_SIZE] = (delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE]);
                        }
                        else if (type == 2) {
                            packetOut.writeBuf[id] = (delay[i++] ^ M17_SCRAMBLER[outCount - M17_SYNC_SIZE]);
                        }
    
                        outCount++;
                    }
                    
                    if (outCount >= M17_RAW_FRAME_SIZE) {
                        detect = false;
                        if (type == 0) {
                            if (!linkSetupOut.swap(M17_CUT_FRAME_SIZE)) { return -1; }
                        }
                        else if (type == 1) {
                            if (!lichOut.swap(12)) {return -1; }
                            if (!streamOut.swap(M17_CUT_FRAME_SIZE)) {return -1; }
                        }
                        else if (type == 2) {
                            if (!packetOut.swap(M17_CUT_FRAME_SIZE)) { return -1; }
                        }
                    }
                    continue;
                }

                // Check for link setup syncword
                if (!memcmp(&delay[i], M17_LSF_SYNC, M17_SYNC_SIZE)) {
                    detect = true;
                    outCount = 0;
                    type = 0;
                    spdlog::warn("Found sync frame");
                    continue;
                }

                // Check for stream syncword
                if (!memcmp(&delay[i], M17_STF_SYNC, M17_SYNC_SIZE)) {
                    detect = true;
                    outCount = 0;
                    type = 1;
                    memset(lichOut.writeBuf, 0, 12);
                    spdlog::warn("Found stream frame");
                    continue;
                }

                // Check for packet syncword
                if (!memcmp(&delay[i], M17_PKF_SYNC, M17_SYNC_SIZE)) {
                    detect = true;
                    outCount = 0;
                    type = 2;
                    spdlog::warn("Found packet frame");
                    continue;
                }

                i++;
            }
            
            memmove(delay, &delay[count], 16);

            _in->flush();

            return count;
        }

        stream<uint8_t> linkSetupOut;
        stream<uint8_t> lichOut;
        stream<uint8_t> streamOut;
        stream<uint8_t> packetOut;

    private:
        stream<uint8_t>* _in;

        uint8_t* delay;

        bool detect = false;
        int type;

        int outCount = 0;

    };

    class M17PayloadFEC : public generic_block<M17PayloadFEC> {
    public:
        M17PayloadFEC() {}

        M17PayloadFEC(stream<uint8_t>* in) { init(in); }

        ~M17PayloadFEC() {
            if (!generic_block<M17PayloadFEC>::_block_init) { return; }
            generic_block<M17PayloadFEC>::stop();
            correct_convolutional_destroy(conv);
        }

        void init(stream<uint8_t>* in) {
            _in = in;
            
            conv = correct_convolutional_create(2, 5, correct_conv_m17_polynomial);

            generic_block<M17PayloadFEC>::registerInput(_in);
            generic_block<M17PayloadFEC>::registerOutput(&out);
            generic_block<M17PayloadFEC>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<M17PayloadFEC>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<M17PayloadFEC>::ctrlMtx);
            generic_block<M17PayloadFEC>::tempStop();
            generic_block<M17PayloadFEC>::unregisterInput(_in);
            _in = in;
            generic_block<M17PayloadFEC>::registerInput(_in);
            generic_block<M17PayloadFEC>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Depuncture the data
            int inOffset = 0;
            for (int i = 0; i < M17_ENCODED_PAYLOAD_SIZE; i++) {
                if (!M17_PUNCTURING_P2[i % 12]) {
                    depunctured[i] = 0;
                    continue;
                }
                depunctured[i] = _in->readBuf[inOffset++];
            }

            // Pack into bytes
            for (int i = 0; i < M17_ENCODED_PAYLOAD_SIZE; i++) {
                if (!(i%8)) { packed[i/8] = 0; }
                packed[i/8] |= depunctured[i] << (7 - (i%8));
            }

            // Run through convolutional decoder
            correct_convolutional_decode(conv, packed, M17_ENCODED_PAYLOAD_SIZE, out.writeBuf);
            
            _in->flush();

            if (!out.swap(M17_PAYLOAD_SIZE / 8)) { return -1; }
            return count;
        }

        stream<uint8_t> out;

    private:
        stream<uint8_t>* _in;

        uint8_t depunctured[296];
        uint8_t packed[37];

        correct_convolutional* conv;

    };

    class M17Codec2Decode : public generic_block<M17Codec2Decode> {
    public:
        M17Codec2Decode() {}

        M17Codec2Decode(stream<uint8_t>* in) { init(in); }

        ~M17Codec2Decode() {
            if (!generic_block<M17Codec2Decode>::_block_init) { return; }
            generic_block<M17Codec2Decode>::stop();
            codec2_destroy(codec);
            delete[] int16Audio;
            delete[] floatAudio;
        }

        void init(stream<uint8_t>* in) {
            _in = in;

            codec = codec2_create(CODEC2_MODE_3200);
            sampsPerC2Frame = codec2_samples_per_frame(codec);
            sampsPerC2FrameDouble = sampsPerC2Frame * 2;
            int16Audio = new int16_t[sampsPerC2FrameDouble];
            floatAudio = new float[sampsPerC2FrameDouble];

            generic_block<M17Codec2Decode>::registerInput(_in);
            generic_block<M17Codec2Decode>::registerOutput(&out);
            generic_block<M17Codec2Decode>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<M17Codec2Decode>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<M17Codec2Decode>::ctrlMtx);
            generic_block<M17Codec2Decode>::tempStop();
            generic_block<M17Codec2Decode>::unregisterInput(_in);
            _in = in;
            generic_block<M17Codec2Decode>::registerInput(_in);
            generic_block<M17Codec2Decode>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            // Decode both parts using codec
            codec2_decode(codec, int16Audio, &_in->readBuf[2]);
            codec2_decode(codec, &int16Audio[sampsPerC2Frame], &_in->readBuf[2+8]);

            // Convert to float
            volk_16i_s32f_convert_32f(floatAudio, int16Audio, 32768.0f, sampsPerC2FrameDouble);

            // Interleave into stereo samples
            volk_32f_x2_interleave_32fc((lv_32fc_t*)out.writeBuf, floatAudio, floatAudio, sampsPerC2FrameDouble);

            _in->flush();

            if (!out.swap(sampsPerC2FrameDouble)) { return -1; }
            return count;
        }

        stream<stereo_t> out;

    private:
        stream<uint8_t>* _in;

        int16_t* int16Audio;
        float* floatAudio;

        CODEC2* codec;
        int sampsPerC2Frame = 0;
        int sampsPerC2FrameDouble = 0;

    };

    class M17Decoder : public generic_hier_block<M17Decoder> {
    public:
        M17Decoder() {}

        M17Decoder(stream<complex_t>* input, float sampleRate) {
            init(input, sampleRate);
        }

        void init(stream<complex_t>* input, float sampleRate) {
            _sampleRate = sampleRate;

            demod.init(input, _sampleRate, M17_DEVIATION);
            rrc.init(31, _sampleRate, M17_BAUDRATE, M17_RRC_ALPHA);
            fir.init(&demod.out, &rrc);
            recov.init(&fir.out, _sampleRate / M17_BAUDRATE, 1e-6f, 0.01f, 0.01f);
            doubler.init(&recov.out);
            slice.init(&doubler.outA);
            demux.init(&slice.out);
            payloadFEC.init(&demux.streamOut);
            decodeAudio.init(&payloadFEC.out);

            ns0.init(&demux.linkSetupOut);
            ns1.init(&demux.lichOut);
            ns2.init(&demux.packetOut);

            diagOut = &doubler.outB;
            out = &decodeAudio.out;
            
            generic_hier_block<M17Decoder>::registerBlock(&demod);
            generic_hier_block<M17Decoder>::registerBlock(&fir);
            generic_hier_block<M17Decoder>::registerBlock(&recov);
            generic_hier_block<M17Decoder>::registerBlock(&doubler);
            generic_hier_block<M17Decoder>::registerBlock(&slice);
            generic_hier_block<M17Decoder>::registerBlock(&demux);
            generic_hier_block<M17Decoder>::registerBlock(&payloadFEC);
            generic_hier_block<M17Decoder>::registerBlock(&decodeAudio);

            generic_hier_block<M17Decoder>::registerBlock(&ns0);
            generic_hier_block<M17Decoder>::registerBlock(&ns1);
            generic_hier_block<M17Decoder>::registerBlock(&ns2);

            generic_hier_block<M17Decoder>::_block_init = true;
        }

        void setInput(stream<complex_t>* input) {
            assert(generic_hier_block<M17Decoder>::_block_init);
            demod.setInput(input);
        }

        stream<float>* diagOut = NULL;
        stream<stereo_t>* out = NULL;

    private:
        FloatFMDemod demod;
        RRCTaps rrc;
        FIR<float> fir;
        MMClockRecovery<float> recov;
        StreamDoubler<float> doubler;
        M17Slice4FSK slice;
        M17FrameDemux demux;
        M17PayloadFEC payloadFEC;
        M17Codec2Decode decodeAudio;

        NullSink<uint8_t> ns0;
        NullSink<uint8_t> ns1;
        NullSink<uint8_t> ns2;


        float _sampleRate;        

    };
}