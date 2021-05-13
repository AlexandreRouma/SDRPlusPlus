#include "fft_mt_r2iq.h"
#include "sddc_config.h"
#include "fftw3.h"
#include "RadioHandler.h"

void * fft_mt_r2iq::r2iqThreadf_def(r2iqThreadArg *th)
{
    #include "fft_mt_r2iq_impl.hpp"
}