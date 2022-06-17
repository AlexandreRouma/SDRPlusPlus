#pragma once
#include "../sink.h"

namespace dsp::bench {
    template<class T>
    class PeakLevelMeter : public Sink<T> {
        using base_type = Sink<T>;
    public:
        PeakLevelMeter() {}

        PeakLevelMeter(stream<T>* in) { init(in); }

        void init(stream<T>* in) {
            if constexpr (std::is_same_v<T, float>) {
                level = 0.0f;
            }
            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                level = { 0.0f, 0.0f };
            }
            base_type::init(in);
        }

        T getLevel() {
            return level;
        }

        void resetLevel() {
            assert(base_type::_block_init);
            if constexpr (std::is_same_v<T, float>) {
                level = 0.0f;
            }
            if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                level = { 0.0f, 0.0f };
            }
        }

        int process(int count, T* in) {
            for (int i = 0; i < count; i++) {
                if constexpr (std::is_same_v<T, float>) {
                    float lvl = fabsf(in[i]);
                    if (lvl > level) { level = lvl; }
                }
                if constexpr (std::is_same_v<T, complex_t>) {
                    float lvlre = fabsf(in[i].re);
                    float lvlim = fabsf(in[i].im);
                    if (lvlre > level.re) { level.re = lvlre; }
                    if (lvlim > level.im) { level.im = lvlim; }
                }
                if constexpr (std::is_same_v<T, stereo_t>) {
                    float lvll = fabsf(in[i].l);
                    float lvlr = fabsf(in[i].r);
                    if (lvll > level.l) { level.l = lvll; }
                    if (lvlr > level.r) { level.r = lvlr; }
                }
            }
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf);

            base_type::_in->flush();
            return count;
        }

    protected:
        T level;
    };
}