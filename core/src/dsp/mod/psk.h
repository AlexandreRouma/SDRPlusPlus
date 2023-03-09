#pragma once
#include "../multirate/rrc_interpolator.h"

namespace dsp::mod {
    // TODO: Check if resample before RRC is better than using the RRC taps as a filter (bandwidth probably not correct for alias-free resampling)
    typedef multirate::RRCInterpolator<complex_t> PSK;
}