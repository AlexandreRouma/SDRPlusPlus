#pragma once
#include "../multirate/rrc_interpolator.h"

namespace dsp::mod {
    typedef multirate::RRCInterpolator<complex_t> PSK;
}