#pragma once
#include <dsp/block.h>
#include <dsp/utils/bitstream.h>

namespace dsp {
    namespace meteor {
        class HRPTDemux : public generic_block<HRPTDemux> {
        public:
            HRPTDemux() {}

            HRPTDemux(stream<uint8_t>* in) { init(in); }

            void init(stream<uint8_t>* in) {
                _in = in;
                generic_block<HRPTDemux>::registerInput(_in);
                generic_block<HRPTDemux>::registerOutput(&telemOut);
                generic_block<HRPTDemux>::registerOutput(&BISMout);
                generic_block<HRPTDemux>::registerOutput(&SSPDOut);
                generic_block<HRPTDemux>::registerOutput(&MTVZAOut);
                generic_block<HRPTDemux>::registerOutput(&MSUMROut);
                generic_block<HRPTDemux>::_block_init = true;
            }

            void setInput(stream<uint8_t>* in) {
                assert(generic_block<HRPTDemux>::_block_init);
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

                for (int i = 0; i < 4; i++) {
                    memcpy(telemOut.writeBuf + (i * 2), _in->readBuf + 4 + (i * 256), 2);
                    memcpy(BISMout.writeBuf + (i * 4), _in->readBuf + 4 + (i * 256) + 2, 4);
                    memcpy(SSPDOut.writeBuf + (i * 4), _in->readBuf + 4 + (i * 256) + 6, 4);
                    memcpy(MTVZAOut.writeBuf + (i * 8), _in->readBuf + 4 + (i * 256) + 10, 8);
                    memcpy(MSUMROut.writeBuf + (i * 238), _in->readBuf + 4 + (i * 256) + 18, (i == 3) ? 234 : 238);
                }

                if (!telemOut.swap(8)) { return -1; }
                if (!BISMout.swap(16)) { return -1; }
                if (!SSPDOut.swap(16)) { return -1; }
                if (!MTVZAOut.swap(32)) { return -1; }
                if (!MSUMROut.swap(948)) { return -1; }

                _in->flush();
                return count;
            }

            stream<uint8_t> telemOut;
            stream<uint8_t> BISMout;
            stream<uint8_t> SSPDOut;
            stream<uint8_t> MTVZAOut;
            stream<uint8_t> MSUMROut;

        private:
            stream<uint8_t>* _in;
        };
    }
}