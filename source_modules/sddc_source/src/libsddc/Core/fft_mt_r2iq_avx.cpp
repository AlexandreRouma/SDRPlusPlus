#include "fft_mt_r2iq.h"
#include "config.h"
#include "fftw3.h"
#include "RadioHandler.h"

void * fft_mt_r2iq::r2iqThreadf_avx(r2iqThreadArg *th)
{
    #include "fft_mt_r2iq_impl.hpp"
}