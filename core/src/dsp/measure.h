#pragma once
#include <dsp/block.h>
#include <fftw3.h>
#include <volk/volk.h>
#include <spdlog/spdlog.h>
#include <dsp/types.h>
#include <string.h>

namespace dsp {
    class LevelMeter : public generic_block<LevelMeter> {
    public:
        LevelMeter() {}

        LevelMeter(stream<stereo_t>* in) { init(in); }

        void init(stream<stereo_t>* in) {
            _in = in;
            generic_block<LevelMeter>::registerInput(_in);
            generic_block<LevelMeter>::_block_init = true;
        }

        void setInput(stream<stereo_t>* in) {
            assert(generic_block<LevelMeter>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<LevelMeter>::ctrlMtx);
            generic_block<LevelMeter>::tempStop();
            generic_block<LevelMeter>::unregisterInput(_in);
            _in = in;
            generic_block<LevelMeter>::registerInput(_in);
            generic_block<LevelMeter>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            float maxL = 0, maxR = 0;
            float absL, absR;
            for (int i = 0; i < count; i++) {
                absL = fabs(_in->readBuf[i].l);
                absR = fabs(_in->readBuf[i].r);
                if (absL > maxL) { maxL = absL; }
                if (absR > maxR) { maxR = absR; }
            }

            _in->flush();

            float _lvlL = 10.0f * logf(maxL);
            float _lvlR = 10.0f * logf(maxR);
            
            // Update max values
            {
                std::lock_guard<std::mutex> lck(lvlMtx);
                if (_lvlL > lvlL) { lvlL = _lvlL; }
                if (_lvlR > lvlR) { lvlR = _lvlR; }
            }
            

            return count;
        }

        float getLeftLevel() {
            std::lock_guard<std::mutex> lck(lvlMtx);
            float ret = lvlL;
            lvlL = -90.0f;
            return ret;
        }

        float getRightLevel() {
            std::lock_guard<std::mutex> lck(lvlMtx);
            float ret = lvlR;
            lvlR = -90.0f;
            return ret;
        }

    private:
        float lvlL = -90.0f;
        float lvlR = -90.0f;
        stream<stereo_t>* _in;
        std::mutex lvlMtx;

    };
}