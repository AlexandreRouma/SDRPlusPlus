#pragma once
#include "../processor.h"

// TODO: Rewrite better!!!!!
namespace dsp::noise_reduction {
    class Squelch : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        Squelch() {}

        Squelch(stream<complex_t>* in, double level) {}

        void init(stream<complex_t>* in, double level) {
            _level = level;
            base_type::init(in);
        }

        void setLevel(double level) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _level = level;
        }

        inline int process(int count, const complex_t* in, complex_t* out) {
            float sum;
            volk_32fc_magnitude_32f(normBuffer, (lv_32fc_t*)in, count);
            volk_32f_accumulator_s32f(&sum, normBuffer, count);
            sum /= (float)count;

            if (10.0f * log10f(sum) >= _level) {
                memcpy(out, in, count * sizeof(complex_t));
            }
            else {
                memset(out, 0, count * sizeof(complex_t));
            }

            return count;
        }

        //DEFAULT_PROC_RUN();

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            process(count, base_type::_in->readBuf, base_type::out.writeBuf);
            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    private:
        float* normBuffer;
        float _level = -50.0f;
                
    };
}