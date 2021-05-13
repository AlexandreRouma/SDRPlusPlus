#pragma once

#include "r2iq.h"
#include "fftw3.h"
#include "sddc_config.h"
#include <algorithm>
#include <string.h>

// use up to this many threads
#define N_MAX_R2IQ_THREADS 1
#define PRINT_INPUT_RANGE  0

static const int halfFft = FFTN_R_ADC / 2;    // half the size of the first fft at ADC 64Msps real rate (2048)
static const int fftPerBuf = transferSize / sizeof(short) / (3 * halfFft / 2) + 1; // number of ffts per buffer with 256|768 overlap

class fft_mt_r2iq : public r2iqControlClass
{
public:
    fft_mt_r2iq();
    virtual ~fft_mt_r2iq();

    float setFreqOffset(float offset);

    void Init(float gain, ringbuffer<int16_t>* buffers, ringbuffer<float>* obuffers);
    void TurnOn();
    void TurnOff(void);
    bool IsOn(void);

protected:

    template<bool rand> void convert_float(const int16_t *input, float* output, int size)
    {
        for(int m = 0; m < size; m++)
        {
            int16_t val;
            if (rand && (input[m] & 1))
            {
                val = input[m] ^ (-2);
            }
            else
            {
                val = input[m];
            }
            output[m] = float(val);
        }
    }

    void shift_freq(fftwf_complex* dest, const fftwf_complex* source1, const fftwf_complex* source2, int start, int end)
    {
        for (int m = start; m < end; m++)
        {
            // besides circular shift, do complex multiplication with the lowpass filter's spectrum
            dest[m][0] = source1[m][0] * source2[m][0] - source1[m][1] * source2[m][1];
            dest[m][1] = source1[m][1] * source2[m][0] + source1[m][0] * source2[m][1];
        }
    }

    template<bool flip> void copy(fftwf_complex* dest, const fftwf_complex* source, int count)
    {
        if (flip)
        {
            for (int i = 0; i < count; i++)
            {
                dest[i][0] = source[i][0];
                dest[i][1] = -source[i][1];
            }
        }
        else
        {
            for (int i = 0; i < count; i++)
            {
                dest[i][0] = source[i][0];
                dest[i][1] = source[i][1];
            }
        }
    }

private:
    ringbuffer<int16_t>* inputbuffer;    // pointer to input buffers
    ringbuffer<float>* outputbuffer;    // pointer to ouput buffers
    int bufIdx;         // index to next buffer to be processed
    r2iqThreadArg* lastThread;

    float GainScale;
    int mfftdim [NDECIDX]; // FFT N dimensions: mfftdim[k] = halfFft / 2^k
    int mtunebin;

    void *r2iqThreadf(r2iqThreadArg *th);   // thread function

    void * r2iqThreadf_def(r2iqThreadArg *th);
    void * r2iqThreadf_avx(r2iqThreadArg *th);
    void * r2iqThreadf_avx2(r2iqThreadArg *th);
    void * r2iqThreadf_avx512(r2iqThreadArg *th);

    fftwf_complex **filterHw;       // Hw complex to each decimation ratio

	fftwf_plan plan_t2f_r2c;          // fftw plan buffers Freq to Time complex to complex per decimation ratio
	fftwf_plan *plan_f2t_c2c;          // fftw plan buffers Time to Freq real to complex per buffer
	fftwf_plan plans_f2t_c2c[NDECIDX];

    uint32_t processor_count;
    r2iqThreadArg* threadArgs[N_MAX_R2IQ_THREADS];
    std::mutex mutexR2iqControl;                   // r2iq control lock
    std::thread r2iq_thread[N_MAX_R2IQ_THREADS]; // thread pointers
};

// assure, that ADC is not oversteered?
struct r2iqThreadArg {

	r2iqThreadArg()
	{
#if PRINT_INPUT_RANGE
		MinMaxBlockCount = 0;
		MinValue = 0;
		MaxValue = 0;
#endif
	}

	float *ADCinTime;                // point to each threads input buffers [nftt][n]
	fftwf_complex *ADCinFreq;         // buffers in frequency
	fftwf_complex *inFreqTmp;         // tmp decimation output buffers (after tune shift)
#if PRINT_INPUT_RANGE
	int MinMaxBlockCount;
	int16_t MinValue;
	int16_t MaxValue;
#endif
};