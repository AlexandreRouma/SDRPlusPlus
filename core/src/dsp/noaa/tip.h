#pragma once
#include <dsp/block.h>
#include <dsp/utils/bitstream.h>

namespace dsp {
    namespace noaa {
        class TIPDemux : public generic_block<TIPDemux> {
        public:
            TIPDemux() {}

            TIPDemux(stream<uint8_t>* in) { init(in); }

            void init(stream<uint8_t>* in) {
                _in = in;
                generic_block<TIPDemux>::registerInput(_in);
                generic_block<TIPDemux>::registerOutput(&HIRSOut);
                generic_block<TIPDemux>::registerOutput(&SEMOut);
                generic_block<TIPDemux>::registerOutput(&DCSOut);
                generic_block<TIPDemux>::registerOutput(&SBUVOut);
                generic_block<TIPDemux>::_block_init = true;
            }

            void setInput(stream<uint8_t>* in) {
                assert(generic_block<TIPDemux>::_block_init);
                std::lock_guard<std::mutex> lck(generic_block<TIPDemux>::ctrlMtx);
                generic_block<TIPDemux>::tempStop();
                generic_block<TIPDemux>::unregisterInput(_in);
                _in = in;
                generic_block<TIPDemux>::registerInput(_in);
                generic_block<TIPDemux>::tempStart();
            }

            int run() {
                int count = _in->read();
                if (count < 0) { return -1; }

                // Extract HIRS
                HIRSOut.writeBuf[0] = _in->readBuf[16];
                HIRSOut.writeBuf[1] = _in->readBuf[17];
                HIRSOut.writeBuf[2] = _in->readBuf[22];
                HIRSOut.writeBuf[3] = _in->readBuf[23];
                HIRSOut.writeBuf[4] = _in->readBuf[26];
                HIRSOut.writeBuf[5] = _in->readBuf[27];
                HIRSOut.writeBuf[6] = _in->readBuf[30];
                HIRSOut.writeBuf[7] = _in->readBuf[31];
                HIRSOut.writeBuf[8] = _in->readBuf[34];
                HIRSOut.writeBuf[9] = _in->readBuf[35];
                HIRSOut.writeBuf[10] = _in->readBuf[38];
                HIRSOut.writeBuf[11] = _in->readBuf[39];
                HIRSOut.writeBuf[12] = _in->readBuf[42];
                HIRSOut.writeBuf[13] = _in->readBuf[43];
                HIRSOut.writeBuf[14] = _in->readBuf[54];
                HIRSOut.writeBuf[15] = _in->readBuf[55];
                HIRSOut.writeBuf[16] = _in->readBuf[58];
                HIRSOut.writeBuf[17] = _in->readBuf[59];
                HIRSOut.writeBuf[18] = _in->readBuf[62];
                HIRSOut.writeBuf[19] = _in->readBuf[63];
                HIRSOut.writeBuf[20] = _in->readBuf[66];
                HIRSOut.writeBuf[21] = _in->readBuf[67];
                HIRSOut.writeBuf[22] = _in->readBuf[70];
                HIRSOut.writeBuf[23] = _in->readBuf[71];
                HIRSOut.writeBuf[24] = _in->readBuf[74];
                HIRSOut.writeBuf[25] = _in->readBuf[75];
                HIRSOut.writeBuf[26] = _in->readBuf[78];
                HIRSOut.writeBuf[27] = _in->readBuf[79];
                HIRSOut.writeBuf[28] = _in->readBuf[82];
                HIRSOut.writeBuf[29] = _in->readBuf[83];
                HIRSOut.writeBuf[30] = _in->readBuf[84];
                HIRSOut.writeBuf[31] = _in->readBuf[85];
                HIRSOut.writeBuf[32] = _in->readBuf[88];
                HIRSOut.writeBuf[33] = _in->readBuf[89];
                HIRSOut.writeBuf[34] = _in->readBuf[92];
                HIRSOut.writeBuf[35] = _in->readBuf[93];
                if (!HIRSOut.swap(36)) { return -1; };

                // Extract SEM
                SEMOut.writeBuf[0] = _in->readBuf[20];
                SEMOut.writeBuf[1] = _in->readBuf[21];
                if (!SEMOut.swap(2)) { return -1; };

                // Extract DCS
                DCSOut.writeBuf[0] = _in->readBuf[18];
                DCSOut.writeBuf[1] = _in->readBuf[19];
                DCSOut.writeBuf[2] = _in->readBuf[24];
                DCSOut.writeBuf[3] = _in->readBuf[25];
                DCSOut.writeBuf[4] = _in->readBuf[28];
                DCSOut.writeBuf[5] = _in->readBuf[29];
                DCSOut.writeBuf[6] = _in->readBuf[32];
                DCSOut.writeBuf[7] = _in->readBuf[33];
                DCSOut.writeBuf[8] = _in->readBuf[40];
                DCSOut.writeBuf[9] = _in->readBuf[41];
                DCSOut.writeBuf[10] = _in->readBuf[44];
                DCSOut.writeBuf[11] = _in->readBuf[45];
                DCSOut.writeBuf[12] = _in->readBuf[52];
                DCSOut.writeBuf[13] = _in->readBuf[53];
                DCSOut.writeBuf[14] = _in->readBuf[56];
                DCSOut.writeBuf[15] = _in->readBuf[57];
                DCSOut.writeBuf[16] = _in->readBuf[60];
                DCSOut.writeBuf[17] = _in->readBuf[61];
                DCSOut.writeBuf[18] = _in->readBuf[64];
                DCSOut.writeBuf[19] = _in->readBuf[65];
                DCSOut.writeBuf[20] = _in->readBuf[68];
                DCSOut.writeBuf[21] = _in->readBuf[69];
                DCSOut.writeBuf[22] = _in->readBuf[72];
                DCSOut.writeBuf[23] = _in->readBuf[73];
                DCSOut.writeBuf[24] = _in->readBuf[76];
                DCSOut.writeBuf[25] = _in->readBuf[77];
                DCSOut.writeBuf[26] = _in->readBuf[86];
                DCSOut.writeBuf[27] = _in->readBuf[87];
                DCSOut.writeBuf[28] = _in->readBuf[90];
                DCSOut.writeBuf[29] = _in->readBuf[91];
                DCSOut.writeBuf[30] = _in->readBuf[94];
                DCSOut.writeBuf[31] = _in->readBuf[95];
                if (!DCSOut.swap(32)) { return -1; };

                // Extract SBUV
                SBUVOut.writeBuf[0] = _in->readBuf[36];
                SBUVOut.writeBuf[1] = _in->readBuf[37];
                SBUVOut.writeBuf[2] = _in->readBuf[80];
                SBUVOut.writeBuf[3] = _in->readBuf[81];
                if (!SBUVOut.swap(4)) { return -1; };


                _in->flush();
                return count;
            }

            stream<uint8_t> HIRSOut;
            stream<uint8_t> SEMOut;
            stream<uint8_t> DCSOut;
            stream<uint8_t> SBUVOut;

        private:
            stream<uint8_t>* _in;

        };

        inline uint16_t HIRSSignedToUnsigned(uint16_t n) {
            return (n & 0x1000) ? (0x1000 + (n & 0xFFF)) : (0xFFF - (n & 0xFFF));
        }

        class HIRSDemux : public generic_block<HIRSDemux> {
        public:
            HIRSDemux() {}

            HIRSDemux(stream<uint8_t>* in) { init(in); }

            void init(stream<uint8_t>* in) {
                _in = in;
                generic_block<HIRSDemux>::registerInput(_in);
                for (int i = 0; i < 20; i++) {
                    generic_block<HIRSDemux>::registerOutput(&radChannels[i]);
                }

                // Clear buffers
                for (int i = 0; i < 20; i++) {
                    for (int j = 0; j < 56; j++) { radChannels[i].writeBuf[j] = 0xFFF; }
                }
            }

            void setInput(stream<uint8_t>* in) {
                std::lock_guard<std::mutex> lck(generic_block<HIRSDemux>::ctrlMtx);
                generic_block<HIRSDemux>::tempStop();
                generic_block<HIRSDemux>::unregisterInput(_in);
                _in = in;
                generic_block<HIRSDemux>::registerInput(_in);
                generic_block<HIRSDemux>::tempStart();
            }

            int run() {
                int count = _in->read();
                if (count < 0) { return -1; }

                int element = readBits(19, 6, _in->readBuf);

                // If we've skipped or are on a non image element and there's data avilable, send it
                if ((element < lastElement || element > 55) && newImageData) {
                    newImageData = false;
                    for (int i = 0; i < 20; i++) {
                        if (!radChannels[i].swap(56)) { return -1; }
                    }

                    // Clear buffers
                    for (int i = 0; i < 20; i++) {
                        for (int j = 0; j < 56; j++) { radChannels[i].writeBuf[j] = 0xFFF; }
                    }
                }
                lastElement = element;

                // If data is part of a line, save it
                if (element <= 55) {
                    newImageData = true;

                    radChannels[0].writeBuf[element] = HIRSSignedToUnsigned(readBits(26, 13, _in->readBuf));
                    radChannels[1].writeBuf[element] = HIRSSignedToUnsigned(readBits(52, 13, _in->readBuf));
                    radChannels[2].writeBuf[element] = HIRSSignedToUnsigned(readBits(65, 13, _in->readBuf));
                    radChannels[3].writeBuf[element] = HIRSSignedToUnsigned(readBits(91, 13, _in->readBuf));
                    radChannels[4].writeBuf[element] = HIRSSignedToUnsigned(readBits(221, 13, _in->readBuf));
                    radChannels[5].writeBuf[element] = HIRSSignedToUnsigned(readBits(208, 13, _in->readBuf));
                    radChannels[6].writeBuf[element] = HIRSSignedToUnsigned(readBits(143, 13, _in->readBuf));
                    radChannels[7].writeBuf[element] = HIRSSignedToUnsigned(readBits(156, 13, _in->readBuf));
                    radChannels[8].writeBuf[element] = HIRSSignedToUnsigned(readBits(273, 13, _in->readBuf));
                    radChannels[9].writeBuf[element] = HIRSSignedToUnsigned(readBits(182, 13, _in->readBuf));
                    radChannels[10].writeBuf[element] = HIRSSignedToUnsigned(readBits(119, 13, _in->readBuf));
                    radChannels[11].writeBuf[element] = HIRSSignedToUnsigned(readBits(247, 13, _in->readBuf));
                    radChannels[12].writeBuf[element] = HIRSSignedToUnsigned(readBits(78, 13, _in->readBuf));
                    radChannels[13].writeBuf[element] = HIRSSignedToUnsigned(readBits(195, 13, _in->readBuf));
                    radChannels[14].writeBuf[element] = HIRSSignedToUnsigned(readBits(234, 13, _in->readBuf));
                    radChannels[15].writeBuf[element] = HIRSSignedToUnsigned(readBits(260, 13, _in->readBuf));
                    radChannels[16].writeBuf[element] = HIRSSignedToUnsigned(readBits(39, 13, _in->readBuf));
                    radChannels[17].writeBuf[element] = HIRSSignedToUnsigned(readBits(104, 13, _in->readBuf));
                    radChannels[18].writeBuf[element] = HIRSSignedToUnsigned(readBits(130, 13, _in->readBuf));
                    radChannels[19].writeBuf[element] = HIRSSignedToUnsigned(readBits(169, 13, _in->readBuf));
                }

                // If we are writing the last pixel of a line, send it already
                if (element == 55) {
                    newImageData = false;
                    for (int i = 0; i < 20; i++) {
                        if (!radChannels[i].swap(56)) { return -1; }
                    }

                    // Clear buffers
                    for (int i = 0; i < 20; i++) {
                        for (int j = 0; j < 56; j++) { radChannels[i].writeBuf[j] = 0xFFF; }
                    }
                }

                _in->flush();
                return count;
            }

            stream<uint16_t> radChannels[20];

        private:
            stream<uint8_t>* _in;
            int lastElement = 0;
            bool newImageData = false;

        };
    }
}
