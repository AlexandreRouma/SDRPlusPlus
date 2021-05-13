#include "license.txt"  
/*
The ADC input real stream of 16 bit samples (at Fs = 64 Msps in the example) is converted to:
- 32 Msps float Fs/2 complex stream, or
- 16 Msps float Fs/2 complex stream, or
-  8 Msps float Fs/2 complex stream, or
-  4 Msps float Fs/2 complex stream, or
-  2 Msps float Fs/2 complex stream.
The decimation factor is selectable from HDSDR GUI sampling rate selector

The name r2iq as Real 2 I+Q stream

*/

#include "fft_mt_r2iq.h"
#include "sddc_config.h"
#include "fftw3.h"
#include "RadioHandler.h"

#include "fir.h"

#include <assert.h>
#include <utility>


r2iqControlClass::r2iqControlClass()
{
	r2iqOn = false;
	randADC = false;
	sideband = false;
	mdecimation = 0;
	mratio[0] = 1;  // 1,2,4,8,16
	for (int i = 1; i < NDECIDX; i++)
	{
		mratio[i] = mratio[i - 1] * 2;
	}
}

fft_mt_r2iq::fft_mt_r2iq() :
	r2iqControlClass(),
	filterHw(nullptr)
{
	mtunebin = halfFft / 4;
	mfftdim[0] = halfFft;
	for (int i = 1; i < NDECIDX; i++)
	{
		mfftdim[i] = mfftdim[i - 1] / 2;
	}
	GainScale = 0.0f;

#ifndef NDEBUG
	int mratio = 1;  // 1,2,4,8,16,..
	const float Astop = 120.0f;
	const float relPass = 0.85f;  // 85% of Nyquist should be usable
	const float relStop = 1.1f;   // 'some' alias back into transition band is OK
	printf("\n***************************************************************************\n");
	printf("Filter tap estimation, Astop = %.1f dB, relPass = %.2f, relStop = %.2f\n", Astop, relPass, relStop);
	for (int d = 0; d < NDECIDX; d++)
	{
		float Bw = 64.0f / mratio;
		int ntaps = KaiserWindow(0, Astop, relPass * Bw / 128.0f, relStop * Bw / 128.0f, nullptr);
		printf("decimation %2d: KaiserWindow(Astop = %.1f dB, Fpass = %.3f,Fstop = %.3f, Bw %.3f @ %f ) => %d taps\n",
			d, Astop, relPass * Bw, relStop * Bw, Bw, 128.0f, ntaps);
		mratio = mratio * 2;
	}
	printf("***************************************************************************\n");
#endif

}

fft_mt_r2iq::~fft_mt_r2iq()
{
	if (filterHw == nullptr)
		return;

	fftwf_export_wisdom_to_filename("wisdom");

	for (int d = 0; d < NDECIDX; d++)
	{
		fftwf_free(filterHw[d]);     // 4096
	}
	fftwf_free(filterHw);

	fftwf_destroy_plan(plan_t2f_r2c);
	for (int d = 0; d < NDECIDX; d++)
	{
		fftwf_destroy_plan(plans_f2t_c2c[d]);
	}

	for (unsigned t = 0; t < processor_count; t++) {
		auto th = threadArgs[t];
		fftwf_free(th->ADCinTime);
		fftwf_free(th->ADCinFreq);
		fftwf_free(th->inFreqTmp);

		delete threadArgs[t];
	}
}


float fft_mt_r2iq::setFreqOffset(float offset)
{
	// align to 1/4 of halfft
	this->mtunebin = int(offset * halfFft / 4) * 4;  // mtunebin step 4 bin  ?
	float delta = ((float)this->mtunebin  / halfFft) - offset;
	float ret = delta * getRatio(); // ret increases with higher decimation
	DbgPrintf("offset %f mtunebin %d delta %f (%f)\n", offset, this->mtunebin, delta, ret);
	return ret;
}

void fft_mt_r2iq::TurnOn() {
	this->r2iqOn = true;
	this->bufIdx = 0;
	this->lastThread = threadArgs[0];

	for (unsigned t = 0; t < processor_count; t++) {
		r2iq_thread[t] = std::thread(
			[this] (void* arg)
				{ return this->r2iqThreadf((r2iqThreadArg*)arg); }, (void*)threadArgs[t]);
	}
}

void fft_mt_r2iq::TurnOff(void) {
	this->r2iqOn = false;

	inputbuffer->Stop();
	outputbuffer->Stop();
	for (unsigned t = 0; t < processor_count; t++) {
		r2iq_thread[t].join();
	}
}

bool fft_mt_r2iq::IsOn(void) { return(this->r2iqOn); }

void fft_mt_r2iq::Init(float gain, ringbuffer<int16_t> *input, ringbuffer<float>* obuffers)
{
	this->inputbuffer = input;    // set to the global exported by main_loop
	this->outputbuffer = obuffers;  // set to the global exported by main_loop

	this->GainScale = gain;

	fftwf_import_wisdom_from_filename("wisdom");

	// Get the processor count
	processor_count = std::thread::hardware_concurrency() - 1;
	if (processor_count == 0)
		processor_count = 1;
	if (processor_count > N_MAX_R2IQ_THREADS)
		processor_count = N_MAX_R2IQ_THREADS;

	{
		fftwf_plan filterplan_t2f_c2c; // time to frequency fft

		DbgPrintf((char *) "r2iqCntrl initialization\n");


		//        DbgPrintf((char *) "RandTable generated\n");

		   // filters
		fftwf_complex *pfilterht;       // time filter ht
		pfilterht = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // halfFft
		filterHw = (fftwf_complex**)fftwf_malloc(sizeof(fftwf_complex*)*NDECIDX);
		for (int d = 0; d < NDECIDX; d++)
		{
			filterHw[d] = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*halfFft);     // halfFft
		}

		filterplan_t2f_c2c = fftwf_plan_dft_1d(halfFft, pfilterht, filterHw[0], FFTW_FORWARD, FFTW_MEASURE);
		float *pht = new float[halfFft / 4 + 1];
		const float Astop = 120.0f;
		const float relPass = 0.85f;  // 85% of Nyquist should be usable
		const float relStop = 1.1f;   // 'some' alias back into transition band is OK
		for (int d = 0; d < NDECIDX; d++)	// @todo when increasing NDECIDX
		{
			// @todo: have dynamic bandpass filter size - depending on decimation
			//   to allow same stopband-attenuation for all decimations
			float Bw = 64.0f / mratio[d];
			// Bw *= 0.8f;  // easily visualize Kaiser filter's response
			KaiserWindow(halfFft / 4 + 1, Astop, relPass * Bw / 128.0f, relStop * Bw / 128.0f, pht);

			float gainadj = gain * 2048.0f / (float)FFTN_R_ADC; // reference is FFTN_R_ADC == 2048

			for (int t = 0; t < halfFft; t++)
			{
				pfilterht[t][0] = pfilterht[t][1]= 0.0F;
			}
		
			for (int t = 0; t < (halfFft/4+1); t++)
			{
				pfilterht[halfFft-1-t][0] = gainadj * pht[t];
			}

			fftwf_execute_dft(filterplan_t2f_c2c, pfilterht, filterHw[d]);
		}
		delete[] pht;
		fftwf_destroy_plan(filterplan_t2f_c2c);
		fftwf_free(pfilterht);

		for (unsigned t = 0; t < processor_count; t++) {
			r2iqThreadArg *th = new r2iqThreadArg();
			threadArgs[t] = th;

			th->ADCinTime = (float*)fftwf_malloc(sizeof(float) * (halfFft + transferSize / 2));                 // 2048

			th->ADCinFreq = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft + 1)); // 1024+1
			th->inFreqTmp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*(halfFft));    // 1024
		}

		plan_t2f_r2c = fftwf_plan_dft_r2c_1d(2 * halfFft, threadArgs[0]->ADCinTime, threadArgs[0]->ADCinFreq, FFTW_MEASURE);
		for (int d = 0; d < NDECIDX; d++)
		{
			plans_f2t_c2c[d] = fftwf_plan_dft_1d(mfftdim[d], threadArgs[0]->inFreqTmp, threadArgs[0]->inFreqTmp, FFTW_BACKWARD, FFTW_MEASURE);
		}
	}
}

#ifdef _WIN32
	//  Windows
	#include <intrin.h>
	#define cpuid(info, x)    __cpuidex(info, x, 0)
#else
	//  GCC Intrinsics
	#include <cpuid.h>
	#define cpuid(info, x)  __cpuid_count(x, 0, info[0], info[1], info[2], info[3])
#endif

void * fft_mt_r2iq::r2iqThreadf(r2iqThreadArg *th)
{
#ifdef NO_SIMD_OPTIM
	DbgPrintf("Hardware Capability: all SIMD features (AVX, AVX2, AVX512) deactivated\n");
	return r2iqThreadf_def(th);
#else
	int info[4];
	bool HW_AVX = false;
	bool HW_AVX2 = false;
	bool HW_AVX512F = false;

	cpuid(info, 0);
	int nIds = info[0];

	if (nIds >= 0x00000001){
		cpuid(info,0x00000001);
		HW_AVX    = (info[2] & ((int)1 << 28)) != 0;
	}
	if (nIds >= 0x00000007){
		cpuid(info,0x00000007);
		HW_AVX2   = (info[1] & ((int)1 <<  5)) != 0;

		HW_AVX512F     = (info[1] & ((int)1 << 16)) != 0;
	}

	DbgPrintf("Hardware Capability: AVX:%d AVX2:%d AVX512:%d\n", HW_AVX, HW_AVX2, HW_AVX512F);

	if (HW_AVX512F)
		return r2iqThreadf_avx512(th);
	else if (HW_AVX2)
		return r2iqThreadf_avx2(th);
	else if (HW_AVX)
		return r2iqThreadf_avx(th);
	else
		return r2iqThreadf_def(th);
#endif
}
