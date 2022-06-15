#pragma once
#include "constants.h"

namespace dsp::math {
    template<class T>
    T normPhaseDiff(T diff) {
        if (diff > FL_M_PI) { diff -= 2.0f * FL_M_PI; }
        else if (diff <= -FL_M_PI) { diff += 2.0f * FL_M_PI; }
        return diff;
    }
}