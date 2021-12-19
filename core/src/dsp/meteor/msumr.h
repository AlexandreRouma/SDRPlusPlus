#pragma once
#include <dsp/block.h>
#include <dsp/utils/bitstream.h>

namespace dsp {
    namespace meteor {
        const uint64_t MSUMR_SYNC_WORD = 0x0218A7A392DD9ABF;
        const uint8_t MSUMR_SYNC_BYTES[8] = { 0x02, 0x18, 0xA7, 0xA3, 0x92, 0xDD, 0x9A, 0xBF };

        class MSUMRDemux : public generic_block<MSUMRDemux> {
        public:
            MSUMRDemux() {}

            MSUMRDemux(stream<uint8_t>* in) { init(in); }

            void init(stream<uint8_t>* in) {
                _in = in;
                generic_block<MSUMRDemux>::registerInput(_in);
                generic_block<MSUMRDemux>::registerOutput(&msumr1Out);
                generic_block<MSUMRDemux>::registerOutput(&msumr2Out);
                generic_block<MSUMRDemux>::registerOutput(&msumr3Out);
                generic_block<MSUMRDemux>::registerOutput(&msumr4Out);
                generic_block<MSUMRDemux>::registerOutput(&msumr5Out);
                generic_block<MSUMRDemux>::registerOutput(&msumr6Out);
                generic_block<MSUMRDemux>::_block_init = true;
            }

            void setInput(stream<uint8_t>* in) {
                assert(generic_block<MSUMRDemux>::_block_init);
                std::lock_guard<std::mutex> lck(generic_block<MSUMRDemux>::ctrlMtx);
                generic_block<MSUMRDemux>::tempStop();
                generic_block<MSUMRDemux>::unregisterInput(_in);
                _in = in;
                generic_block<MSUMRDemux>::registerInput(_in);
                generic_block<MSUMRDemux>::tempStart();
            }

            int run() {
                int count = _in->read();
                if (count < 0) { return -1; }

                int pixels = 0;
                for (int i = 0; i < 11790; i += 30) {
                    for (int j = 0; j < 4; j++) {
                        msumr1Out.writeBuf[pixels + j] = (uint16_t)readBits((50 * 8) + (i * 8) + (j * 10), 10, _in->readBuf);
                        msumr2Out.writeBuf[pixels + j] = (uint16_t)readBits((50 * 8) + (i * 8) + (j * 10) + (40 * 1), 10, _in->readBuf);
                        msumr3Out.writeBuf[pixels + j] = (uint16_t)readBits((50 * 8) + (i * 8) + (j * 10) + (40 * 2), 10, _in->readBuf);
                        msumr4Out.writeBuf[pixels + j] = (uint16_t)readBits((50 * 8) + (i * 8) + (j * 10) + (40 * 3), 10, _in->readBuf);
                        msumr5Out.writeBuf[pixels + j] = (uint16_t)readBits((50 * 8) + (i * 8) + (j * 10) + (40 * 4), 10, _in->readBuf);
                        msumr6Out.writeBuf[pixels + j] = (uint16_t)readBits((50 * 8) + (i * 8) + (j * 10) + (40 * 5), 10, _in->readBuf);
                    }
                    pixels += 4;
                }

                if (!msumr1Out.swap(1572)) { return -1; }
                if (!msumr2Out.swap(1572)) { return -1; }
                if (!msumr3Out.swap(1572)) { return -1; }
                if (!msumr4Out.swap(1572)) { return -1; }
                if (!msumr5Out.swap(1572)) { return -1; }
                if (!msumr6Out.swap(1572)) { return -1; }

                _in->flush();
                return count;
            }

            stream<uint16_t> msumr1Out;
            stream<uint16_t> msumr2Out;
            stream<uint16_t> msumr3Out;
            stream<uint16_t> msumr4Out;
            stream<uint16_t> msumr5Out;
            stream<uint16_t> msumr6Out;

        private:
            stream<uint8_t>* _in;
        };
    }
}