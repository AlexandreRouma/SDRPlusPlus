#pragma once
#include <dsp/block.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/window.h>

namespace dsp {
    class IQCorrector : public generic_block<IQCorrector> {
    public:
        IQCorrector() {}

        IQCorrector(stream<complex_t>* in, float rate) { init(in, rate); }

        void init(stream<complex_t>* in, float rate) {
            _in = in;
            correctionRate = rate;
            offset.re = 0;
            offset.im = 0;
            generic_block<IQCorrector>::registerInput(_in);
            generic_block<IQCorrector>::registerOutput(&out);
            generic_block<IQCorrector>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<IQCorrector>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<IQCorrector>::ctrlMtx);
            generic_block<IQCorrector>::tempStop();
            generic_block<IQCorrector>::unregisterInput(_in);
            _in = in;
            generic_block<IQCorrector>::registerInput(_in);
            generic_block<IQCorrector>::tempStart();
        }

        void setCorrectionRate(float rate) {
            correctionRate = rate;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            if (bypass) {
                memcpy(out.writeBuf, _in->readBuf, count * sizeof(complex_t));

                _in->flush();

                if (!out.swap(count)) { return -1; }

                return count;
            }

            for (int i = 0; i < count; i++) {
                out.writeBuf[i] = _in->readBuf[i] - offset;
                offset = offset + (out.writeBuf[i] * correctionRate);
            }

            _in->flush();

            if (!out.swap(count)) { return -1; }

            return count;
        }

        stream<complex_t> out;

        // TEMPORARY FOR DEBUG PURPOSES
        bool bypass = false;
        complex_t offset;

    private:
        stream<complex_t>* _in;
        float correctionRate = 0.00001;
        

    };
}