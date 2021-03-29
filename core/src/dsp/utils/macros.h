#pragma once
#include <dsp/types.h>

#define DSP_SIGN(n)         ((n) >= 0)
#define DSP_STEP_CPLX(c)    (complex_t{(c.re > 0.0f) ? 1.0f : -1.0f, (c.im > 0.0f) ? 1.0f : -1.0f})
#define DSP_STEP(n)         (((n) > 0.0f) ? 1.0f : -1.0f)