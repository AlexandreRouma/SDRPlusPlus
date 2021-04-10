#pragma once
#include <dsp/block.h>
#include <dsp/utils/bitstream.h>

namespace dsp {
    namespace noaa {
        inline uint16_t HRPTReadWord(int offset, uint8_t* buffer) {
            return (uint16_t)readBits(offset * 10, 10, buffer);
        }

        const uint8_t HRPTSyncWord[] = {
            1,0,1,0,0,0,0,1,0,0,
            0,1,0,1,1,0,1,1,1,1,
            1,1,0,1,0,1,1,1,0,0,
            0,1,1,0,0,1,1,1,0,1,
            1,0,0,0,0,0,1,1,1,1,
            0,0,1,0,0,1,0,1,0,1
        };

        class HRPTDemux : public generic_block<HRPTDemux> {
        public:
            HRPTDemux() {}

            HRPTDemux(stream<uint8_t>* in) { init(in); }

            void init(stream<uint8_t>* in) {
                _in = in;
                generic_block<HRPTDemux>::registerInput(_in);
                generic_block<HRPTDemux>::registerOutput(&AVHRRChan1Out);
                generic_block<HRPTDemux>::registerOutput(&AVHRRChan2Out);
                generic_block<HRPTDemux>::registerOutput(&AVHRRChan3Out);
                generic_block<HRPTDemux>::registerOutput(&AVHRRChan4Out);
                generic_block<HRPTDemux>::registerOutput(&AVHRRChan5Out);
            }

            void setInput(stream<uint8_t>* in) {
                std::lock_guard<std::mutex> lck(generic_block<HRPTDemux>::ctrlMtx);
                generic_block<HRPTDemux>::tempStop();
                generic_block<HRPTDemux>::unregisterInput(_in);
                _in = in;
                generic_block<HRPTDemux>::registerInput(_in);
                generic_block<HRPTDemux>::tempStart();
            }

            int run() {
                int count = _in->read();
                if (count < 0) { return -1; }

                int minFrame = readBits(61, 2, _in->readBuf);

                // If GAC frame, reject
                if (minFrame == 0) {
                    _in->flush();
                    return count;
                }

                // Extract TIP frames if present
                if (minFrame == 1) {
                    for (int i = 0; i < 5; i++) {
                        for (int j = 0; j < 104; j++) {
                            TIPOut.writeBuf[j] = (HRPTReadWord(103 + (i * 104) + j, _in->readBuf) >> 2) & 0xFF;
                        }
                        if (!TIPOut.swap(104)) { return -1; };
                    }
                }

                // Exctact AIP otherwise
                else if (minFrame == 3) {
                    for (int i = 0; i < 5; i++) {
                        for (int j = 0; j < 104; j++) {
                            AIPOut.writeBuf[j] = (HRPTReadWord(103 + (i * 104) + j, _in->readBuf) >> 2) & 0xFF;
                        }
                        if (!AIPOut.swap(104)) { return -1; };
                    }
                }

                // Extract AVHRR data
                for (int i = 0; i < 2048; i++) {
                    AVHRRChan1Out.writeBuf[i] = HRPTReadWord(750 + (i * 5), _in->readBuf);
                    AVHRRChan2Out.writeBuf[i] = HRPTReadWord(750 + (i * 5) + 1, _in->readBuf);
                    AVHRRChan3Out.writeBuf[i] = HRPTReadWord(750 + (i * 5) + 2, _in->readBuf);
                    AVHRRChan4Out.writeBuf[i] = HRPTReadWord(750 + (i * 5) + 3, _in->readBuf);
                    AVHRRChan5Out.writeBuf[i] = HRPTReadWord(750 + (i * 5) + 4, _in->readBuf);
                }
                if (!AVHRRChan1Out.swap(2048)) { return -1; };
                if (!AVHRRChan2Out.swap(2048)) { return -1; };
                if (!AVHRRChan3Out.swap(2048)) { return -1; };
                if (!AVHRRChan4Out.swap(2048)) { return -1; };
                if (!AVHRRChan5Out.swap(2048)) { return -1; };

                _in->flush();
                return count;
            }

            stream<uint8_t> TIPOut;
            stream<uint8_t> AIPOut;

            stream<uint16_t> AVHRRChan1Out;
            stream<uint16_t> AVHRRChan2Out;
            stream<uint16_t> AVHRRChan3Out;
            stream<uint16_t> AVHRRChan4Out;
            stream<uint16_t> AVHRRChan5Out;

        private:
            stream<uint8_t>* _in;

        };
    }
}
