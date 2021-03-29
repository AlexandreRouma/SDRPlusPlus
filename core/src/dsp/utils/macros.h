#pragma once
#include <dsp/types.h>

#define DSP_SIGN(n)         ((n) >= 0)
#define DSP_STEP_CPLX(c)    (complex_t{(c.i > 0.0f) ? 1.0f : -1.0f, (c.q > 0.0f) ? 1.0f : -1.0f})
#define DSP_STEP(n)         (((n) > 0.0f) ? 1.0f : -1.0f)