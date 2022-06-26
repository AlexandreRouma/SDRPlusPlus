#pragma once

namespace dsp::math {
    template <class T>
    inline T step(T x) {
        return (x > 0.0) ? 1.0 : -1.0;
    }
}