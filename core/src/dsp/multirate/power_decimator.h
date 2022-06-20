#pragma once
#include "../filter/decimating_fir.h"
#include "../taps/from_array.h"
#include "decim/plans.h"

namespace dsp::multirate {
    template<class T>
    class PowerDecimator : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        PowerDecimator() {}

        PowerDecimator(stream<T>* in, unsigned int ratio) { init(in, ratio); }

        ~PowerDecimator() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            freeFirs();
        }

        void init(stream<T>* in, unsigned int ratio) {
            assert(checkRatio(ratio));
            _ratio = ratio;
            reconfigure();
            base_type::init(in);
        }

        static inline unsigned int getMaxRatio() {
            return 1 << decim::plans_len;
        }

        void setRatio(unsigned int ratio) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _ratio = ratio;
            reconfigure();
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            for (auto& fir : decimFirs) {
                fir->reset();
            }
            base_type::tempStart();
        }

        inline int process(int count, const T* in, T* out) {
            // If the ratio is 1, no need to decimate
            if (_ratio == 1) {
                memcpy(out, in, count * sizeof(T));
                return count;
            }
            
            // Process data through each stage
            const T* data = in;
            int last = stageCount - 1;
            for (int i = 0; i < stageCount; i++) {
                auto fir = decimFirs[i];
                count = fir->process(count, data, out);
                data = out;
            }
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        void freeFirs() {
            for (auto& fir : decimFirs) { delete fir; }
            for (auto& taps : decimTaps) { taps::free(taps); }
            decimFirs.clear();
            decimTaps.clear();
        }

        void reconfigure() {
            // Delete DDC FIRs and taps
            freeFirs();

            // Generate filters based on DDC plan
            if (_ratio > 1) {
                int planId = log2(_ratio) - 1;
                decim::plan plan = decim::plans[planId];
                stageCount = plan.stageCount;
                for (int i = 0; i < stageCount; i++) {
                    tap<float> taps = taps::fromArray<float>(plan.stages[i].tapcount, plan.stages[i].taps);
                    auto fir = new filter::DecimatingFIR<T, float>(NULL, taps, plan.stages[i].decimation);
                    fir->out.free();
                    decimTaps.push_back(taps);
                    decimFirs.push_back(fir);
                }
            }
        }

        bool checkRatio(unsigned int ratio) {
            // Make sure ratio is a power of two, non-zero and lower or equal to maximum
            return ((ratio & (ratio - 1)) == 0) && ratio && ratio <= getMaxRatio();
        }

        std::vector<filter::DecimatingFIR<T, float>*> decimFirs;
        std::vector<tap<float>> decimTaps;
        unsigned int _ratio;
        int stageCount;
    };
}