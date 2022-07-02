#pragma once
#include "../types.h"

namespace dsp::math {
    template <class T>
    inline T step(T x) {
        // TODO: Switch to cursed bit manipulation instead!
        if constexpr (std::is_same_v<T, complex_t>) {
            return { (x.re > 0.0f) ? 1.0f : -1.0f, (x.im > 0.0f) ? 1.0f : -1.0f };
        }
        else if constexpr (std::is_same_v<T, stereo_t>) {
            return { (x.l > 0.0f) ? 1.0f : -1.0f, (x.r > 0.0f) ? 1.0f : -1.0f };
        }
        else {
            return (x > 0.0) ? 1.0 : -1.0;
        }
    }
}