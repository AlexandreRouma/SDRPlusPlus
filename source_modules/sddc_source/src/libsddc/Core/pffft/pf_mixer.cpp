/*
This software is part of pffft/pfdsp, a set of simple DSP routines.

Copyright (c) 2014, Andras Retzler <randras@sdr.hu>
Copyright (c) 2020  Hayati Ayguen <h_ayguen@web.de>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ANDRAS RETZLER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* include own header first, to see missing includes */
#include "pf_mixer.h"
#include "fmv.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>

//they dropped M_PI in C99, so we define it:
#define PI ((float)3.14159265358979323846)

//apply to pointers:
#define iof(complexf_input_p,i) (*(((float*)complexf_input_p)+2*(i)))
#define qof(complexf_input_p,i) (*(((float*)complexf_input_p)+2*(i)+1))

#define USE_ALIGNED_ADDRESSES  0



/*
  _____   _____ _____      __                  _   _
 |  __ \ / ____|  __ \    / _|                | | (_)
 | |  | | (___ | |__) |  | |_ _   _ _ __   ___| |_ _  ___  _ __  ___
 | |  | |\___ \|  ___/   |  _| | | | '_ \ / __| __| |/ _ \| '_ \/ __|
 | |__| |____) | |       | | | |_| | | | | (__| |_| | (_) | | | \__ \
 |_____/|_____/|_|       |_|  \__,_|_| |_|\___|\__|_|\___/|_| |_|___/

*/


#if defined(__GNUC__)
#  define ALWAYS_INLINE(return_type) inline return_type __attribute__ ((always_inline))
#  define RESTRICT __restrict
#elif defined(_MSC_VER)
#  define ALWAYS_INLINE(return_type) __forceinline return_type
#  define RESTRICT __restrict
#endif


#ifndef PFFFT_SIMD_DISABLE
#if (defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86))
  #pragma message("Manual SSE x86/x64 optimizations are ON")
  #include <xmmintrin.h>
  #define HAVE_SSE_INTRINSICS 1
  
#elif defined(PFFFT_ENABLE_NEON) && defined(__arm__)
  #pragma message "Manual NEON (arm32) optimizations are ON"
  #include "sse2neon.h"
  #define HAVE_SSE_INTRINSICS 1
  
#elif defined(PFFFT_ENABLE_NEON) && defined(__aarch64__)
  #pragma message "Manual NEON (aarch64) optimizations are ON"
  #include "sse2neon.h"
  #define HAVE_SSE_INTRINSICS 1

#endif
#endif

#ifdef HAVE_SSE_INTRINSICS

typedef __m128 v4sf;
#  define SIMD_SZ 4

typedef union v4_union {
  __m128  v;
  float f[4];
} v4_union;

#define VMUL(a,b)                 _mm_mul_ps(a,b)
#define VDIV(a,b)                 _mm_div_ps(a,b)
#define VADD(a,b)                 _mm_add_ps(a,b)
#define VSUB(a,b)                 _mm_sub_ps(a,b)
#define LD_PS1(s)                 _mm_set1_ps(s)
#define VLOAD_UNALIGNED(ptr)      _mm_loadu_ps((const float *)(ptr))
#define VLOAD_ALIGNED(ptr)        _mm_load_ps((const float *)(ptr))
#define VSTORE_UNALIGNED(ptr, v)  _mm_storeu_ps((float*)(ptr), v)
#define VSTORE_ALIGNED(ptr, v)    _mm_store_ps((float*)(ptr), v)
#define INTERLEAVE2(in1, in2, out1, out2) { __m128 tmp__ = _mm_unpacklo_ps(in1, in2); out2 = _mm_unpackhi_ps(in1, in2); out1 = tmp__; }
#define UNINTERLEAVE2(in1, in2, out1, out2) { __m128 tmp__ = _mm_shuffle_ps(in1, in2, _MM_SHUFFLE(2,0,2,0)); out2 = _mm_shuffle_ps(in1, in2, _MM_SHUFFLE(3,1,3,1)); out1 = tmp__; }

#if USE_ALIGNED_ADDRESSES
  #define VLOAD(ptr)              _mm_load_ps((const float *)(ptr))
  #define VSTORE(ptr, v)          _mm_store_ps((float*)(ptr), v)
#else
  #define VLOAD(ptr)              _mm_loadu_ps((const float *)(ptr))
  #define VSTORE(ptr, v)          _mm_storeu_ps((float*)(ptr), v)
#endif


int have_sse_shift_mixer_impl()
{
    return 1;
}

#else

int have_sse_shift_mixer_impl()
{
    return 0;
}

#endif


/*********************************************************************/

/**************/
/*** ALGO A ***/
/**************/

PF_TARGET_CLONES
float shift_math_cc(complexf *input, complexf* output, int input_size, float rate, float starting_phase)
{
    rate*=2;
    //Shifts the complex spectrum. Basically a complex mixer. This version uses cmath.
    float phase=starting_phase;
    float phase_increment=rate*PI;
    float cosval, sinval;
    for(int i=0;i<input_size; i++)
    {
        cosval=cosf(phase);
        sinval=sinf(phase);
        //we multiply two complex numbers.
        //how? enter this to maxima (software) for explanation:
        //   (a+b*%i)*(c+d*%i), rectform;
        iof(output,i)=cosval*iof(input,i)-sinval*qof(input,i);
        qof(output,i)=sinval*iof(input,i)+cosval*qof(input,i);
        phase+=phase_increment;
        while(phase>2*PI) phase-=2*PI; //@shift_math_cc: normalize phase
        while(phase<0) phase+=2*PI;
    }
    return phase;
}

/*********************************************************************/

/**************/
/*** ALGO B ***/
/**************/

shift_table_data_t shift_table_init(int table_size)
{
    shift_table_data_t output;
    output.table=(float*)malloc(sizeof(float)*table_size);
    output.table_size=table_size;
    for(int i=0;i<table_size;i++)
    {
        output.table[i]=sinf(((float)i/table_size)*(PI/2));
    }
    return output;
}

void shift_table_deinit(shift_table_data_t table_data)
{
    free(table_data.table);
}


PF_TARGET_CLONES
float shift_table_cc(complexf* input, complexf* output, int input_size, float rate, shift_table_data_t table_data, float starting_phase)
{
    rate*=2;
    //Shifts the complex spectrum. Basically a complex mixer. This version uses a pre-built sine table.
    float phase=starting_phase;
    float phase_increment=rate*PI;
    float cosval, sinval;
    for(int i=0;i<input_size; i++) //@shift_math_cc
    {
        int sin_index, cos_index, temp_index, sin_sign, cos_sign;
        int quadrant=(int)(phase/(PI/2.0f)); //between 0 and 3
        float vphase=phase-quadrant*(PI/2.0f);
        sin_index=(int)(vphase/(PI/2.0f))*table_data.table_size;
        cos_index=table_data.table_size-1-sin_index;
        if(quadrant&1) //in quadrant 1 and 3
        {
            temp_index=sin_index;
            sin_index=cos_index;
            cos_index=temp_index;
        }
        sin_sign=(quadrant>1)?-1:1; //in quadrant 2 and 3
        cos_sign=(quadrant&&quadrant<3)?-1:1; //in quadrant 1 and 2
        sinval=sin_sign*table_data.table[sin_index];
        cosval=cos_sign*table_data.table[cos_index];
        //we multiply two complex numbers.
        //how? enter this to maxima (software) for explanation:
        //   (a+b*%i)*(c+d*%i), rectform;
        iof(output,i)=cosval*iof(input,i)-sinval*qof(input,i);
        qof(output,i)=sinval*iof(input,i)+cosval*qof(input,i);
        phase+=phase_increment;
        while(phase>2*PI) phase-=2*PI; //@shift_math_cc: normalize phase
        while(phase<0) phase+=2*PI;
    }
    return phase;
}

/*********************************************************************/

/**************/
/*** ALGO C ***/
/**************/

shift_addfast_data_t shift_addfast_init(float rate)
{
    shift_addfast_data_t output;
    output.phase_increment=2*rate*PI;
    for(int i=0;i<4;i++)
    {
        output.dsin[i]=sinf(output.phase_increment*(i+1));
        output.dcos[i]=cosf(output.phase_increment*(i+1));
    }
    return output;
}

#define SADF_L1(j) \
    cos_vals_ ## j = cos_start * dcos_ ## j - sin_start * dsin_ ## j; \
    sin_vals_ ## j = sin_start * dcos_ ## j + cos_start * dsin_ ## j;
#define SADF_L2(j) \
    iof(output,4*i+j)=(cos_vals_ ## j)*iof(input,4*i+j)-(sin_vals_ ## j)*qof(input,4*i+j); \
    qof(output,4*i+j)=(sin_vals_ ## j)*iof(input,4*i+j)+(cos_vals_ ## j)*qof(input,4*i+j);

PF_TARGET_CLONES
float shift_addfast_cc(complexf *input, complexf* output, int input_size, shift_addfast_data_t* d, float starting_phase)
{
    //input_size should be multiple of 4
    //fprintf(stderr, "shift_addfast_cc: input_size = %d\n", input_size);
    float cos_start=cosf(starting_phase);
    float sin_start=sinf(starting_phase);
    float cos_vals_0, cos_vals_1, cos_vals_2, cos_vals_3,
        sin_vals_0, sin_vals_1, sin_vals_2, sin_vals_3,
        dsin_0 = d->dsin[0], dsin_1 = d->dsin[1], dsin_2 = d->dsin[2], dsin_3 = d->dsin[3],
        dcos_0 = d->dcos[0], dcos_1 = d->dcos[1], dcos_2 = d->dcos[2], dcos_3 = d->dcos[3];

    for(int i=0;i<input_size/4; i++)
    {
        SADF_L1(0)
        SADF_L1(1)
        SADF_L1(2)
        SADF_L1(3)
        SADF_L2(0)
        SADF_L2(1)
        SADF_L2(2)
        SADF_L2(3)
        cos_start = cos_vals_3;
        sin_start = sin_vals_3;
    }
    starting_phase+=input_size*d->phase_increment;
    while(starting_phase>PI) starting_phase-=2*PI;
    while(starting_phase<-PI) starting_phase+=2*PI;
    return starting_phase;
}

#undef SADF_L2


#define SADF_L2(j) \
    tmp_inp_cos = iof(in_out,4*i+j); \
    tmp_inp_sin = qof(in_out,4*i+j); \
    iof(in_out,4*i+j)=(cos_vals_ ## j)*tmp_inp_cos - (sin_vals_ ## j)*tmp_inp_sin; \
    qof(in_out,4*i+j)=(sin_vals_ ## j)*tmp_inp_cos + (cos_vals_ ## j)*tmp_inp_sin;

PF_TARGET_CLONES
float shift_addfast_inp_c(complexf *in_out, int N_cplx, shift_addfast_data_t* d, float starting_phase)
{
    //input_size should be multiple of 4
    //fprintf(stderr, "shift_addfast_cc: input_size = %d\n", input_size);
    float cos_start=cosf(starting_phase);
    float sin_start=sinf(starting_phase);
    float tmp_inp_cos, tmp_inp_sin,
        cos_vals_0, cos_vals_1, cos_vals_2, cos_vals_3,
        sin_vals_0, sin_vals_1, sin_vals_2, sin_vals_3,
        dsin_0 = d->dsin[0], dsin_1 = d->dsin[1], dsin_2 = d->dsin[2], dsin_3 = d->dsin[3],
        dcos_0 = d->dcos[0], dcos_1 = d->dcos[1], dcos_2 = d->dcos[2], dcos_3 = d->dcos[3];

    for(int i=0;i<N_cplx/4; i++)
    {
        SADF_L1(0)
        SADF_L1(1)
        SADF_L1(2)
        SADF_L1(3)
        SADF_L2(0)
        SADF_L2(1)
        SADF_L2(2)
        SADF_L2(3)
        cos_start = cos_vals_3;
        sin_start = sin_vals_3;
    }
    starting_phase+=N_cplx*d->phase_increment;
    while(starting_phase>PI) starting_phase-=2*PI;
    while(starting_phase<-PI) starting_phase+=2*PI;
    return starting_phase;
}

#undef SADF_L1
#undef SADF_L2


/*********************************************************************/

/**************/
/*** ALGO D ***/
/**************/

shift_unroll_data_t shift_unroll_init(float rate, int size)
{
    shift_unroll_data_t output;
    output.phase_increment=2*rate*PI;
    output.size = size;
    output.dsin=(float*)malloc(sizeof(float)*size);
    output.dcos=(float*)malloc(sizeof(float)*size);
    float myphase = 0;
    for(int i=0;i<size;i++)
    {
        myphase += output.phase_increment;
        while(myphase>PI) myphase-=2*PI;
        while(myphase<-PI) myphase+=2*PI;
        output.dsin[i]=sinf(myphase);
        output.dcos[i]=cosf(myphase);
    }
    return output;
}

void shift_unroll_deinit(shift_unroll_data_t* d)
{
    if (!d)
        return;
    free(d->dsin);
    free(d->dcos);
    d->dsin = NULL;
    d->dcos = NULL;
}

PF_TARGET_CLONES
float shift_unroll_cc(complexf *input, complexf* output, int input_size, shift_unroll_data_t* d, float starting_phase)
{
    //input_size should be multiple of 4
    //fprintf(stderr, "shift_addfast_cc: input_size = %d\n", input_size);
    float cos_start = cosf(starting_phase);
    float sin_start = sinf(starting_phase);
    float cos_val = cos_start, sin_val = sin_start;
    for(int i=0;i<input_size; i++)
    {
        iof(output,i) = cos_val*iof(input,i) - sin_val*qof(input,i);
        qof(output,i) = sin_val*iof(input,i) + cos_val*qof(input,i);
        // calculate complex phasor for next iteration
        cos_val = cos_start * d->dcos[i] - sin_start * d->dsin[i];
        sin_val = sin_start * d->dcos[i] + cos_start * d->dsin[i];
    }
    starting_phase+=input_size*d->phase_increment;
    while(starting_phase>PI) starting_phase-=2*PI;
    while(starting_phase<-PI) starting_phase+=2*PI;
    return starting_phase;
}

PF_TARGET_CLONES
float shift_unroll_inp_c(complexf* in_out, int size, shift_unroll_data_t* d, float starting_phase)
{
    float cos_start = cosf(starting_phase);
    float sin_start = sinf(starting_phase);
    float cos_val = cos_start, sin_val = sin_start;
    for(int i=0;i<size; i++)
    {
        float inp_i = iof(in_out,i);
        float inp_q = qof(in_out,i);
        iof(in_out,i) = cos_val*inp_i - sin_val*inp_q;
        qof(in_out,i) = sin_val*inp_i + cos_val*inp_q;
        // calculate complex phasor for next iteration
        cos_val = cos_start * d->dcos[i] - sin_start * d->dsin[i];
        sin_val = sin_start * d->dcos[i] + cos_start * d->dsin[i];
    }
    starting_phase += size * d->phase_increment;
    while(starting_phase>PI) starting_phase-=2*PI;
    while(starting_phase<-PI) starting_phase+=2*PI;
    return starting_phase;
}


/*********************************************************************/

/**************/
/*** ALGO E ***/
/**************/

shift_limited_unroll_data_t shift_limited_unroll_init(float rate)
{
    shift_limited_unroll_data_t output;
    output.phase_increment=2*rate*PI;
    float myphase = 0;
    for(int i=0; i < PF_SHIFT_LIMITED_UNROLL_SIZE; i++)
    {
        myphase += output.phase_increment;
        while(myphase>PI) myphase-=2*PI;
        while(myphase<-PI) myphase+=2*PI;
        output.dcos[i] = cosf(myphase);
        output.dsin[i] = sinf(myphase);
    }
    output.complex_phase.i = 1.0F;
    output.complex_phase.q = 0.0F;
    return output;
}

PF_TARGET_CLONES
void shift_limited_unroll_cc(const complexf *input, complexf* output, int size, shift_limited_unroll_data_t* d)
{
    float cos_start = d->complex_phase.i;
    float sin_start = d->complex_phase.q;
    float cos_val = cos_start, sin_val = sin_start, mag;
    while (size > 0)
    {
        int N = (size >= PF_SHIFT_LIMITED_UNROLL_SIZE) ? PF_SHIFT_LIMITED_UNROLL_SIZE : size;
        for(int i=0;i<N/PF_SHIFT_LIMITED_SIMD_SZ; i++ )
        {
            for(int j=0; j<PF_SHIFT_LIMITED_SIMD_SZ; j++)
            {
                iof(output,PF_SHIFT_LIMITED_SIMD_SZ*i+j) = cos_val*iof(input,PF_SHIFT_LIMITED_SIMD_SZ*i+j) - sin_val*qof(input,PF_SHIFT_LIMITED_SIMD_SZ*i+j);
                qof(output,PF_SHIFT_LIMITED_SIMD_SZ*i+j) = sin_val*iof(input,PF_SHIFT_LIMITED_SIMD_SZ*i+j) + cos_val*qof(input,PF_SHIFT_LIMITED_SIMD_SZ*i+j);
                // calculate complex phasor for next iteration
                cos_val = cos_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] - sin_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
                sin_val = sin_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] + cos_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            }
        }
        // "starts := vals := vals / |vals|"
        mag = sqrtf(cos_val * cos_val + sin_val * sin_val);
        cos_val /= mag;
        sin_val /= mag;
        cos_start = cos_val;
        sin_start = sin_val;

        input += PF_SHIFT_LIMITED_UNROLL_SIZE;
        output += PF_SHIFT_LIMITED_UNROLL_SIZE;
        size -= PF_SHIFT_LIMITED_UNROLL_SIZE;
    }
    d->complex_phase.i = cos_val;
    d->complex_phase.q = sin_val;
}

PF_TARGET_CLONES
void shift_limited_unroll_inp_c(complexf* in_out, int N_cplx, shift_limited_unroll_data_t* d)
{
    float inp_i[PF_SHIFT_LIMITED_SIMD_SZ];
    float inp_q[PF_SHIFT_LIMITED_SIMD_SZ];
    // "vals := starts := phase_state"
    float cos_start = d->complex_phase.i;
    float sin_start = d->complex_phase.q;
    float cos_val = cos_start, sin_val = sin_start, mag;
    while (N_cplx)
    {
        int N = (N_cplx >= PF_SHIFT_LIMITED_UNROLL_SIZE) ? PF_SHIFT_LIMITED_UNROLL_SIZE : N_cplx;
        for(int i=0;i<N/PF_SHIFT_LIMITED_SIMD_SZ; i++ )
        {
            for(int j=0; j<PF_SHIFT_LIMITED_SIMD_SZ; j++)
                inp_i[j] = in_out[PF_SHIFT_LIMITED_SIMD_SZ*i+j].i;
            for(int j=0; j<PF_SHIFT_LIMITED_SIMD_SZ; j++)
                inp_q[j] = in_out[PF_SHIFT_LIMITED_SIMD_SZ*i+j].q;
            for(int j=0; j<PF_SHIFT_LIMITED_SIMD_SZ; j++)
            {
                // "out[] = inp[] * vals"
                iof(in_out,PF_SHIFT_LIMITED_SIMD_SZ*i+j) = cos_val*inp_i[j] - sin_val*inp_q[j];
                qof(in_out,PF_SHIFT_LIMITED_SIMD_SZ*i+j) = sin_val*inp_i[j] + cos_val*inp_q[j];
                // calculate complex phasor for next iteration
                // "vals :=  d[] * starts"
                cos_val = cos_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] - sin_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
                sin_val = sin_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] + cos_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            }
        }
        // "starts := vals := vals / |vals|"
        mag = sqrtf(cos_val * cos_val + sin_val * sin_val);
        cos_val /= mag;
        sin_val /= mag;
        cos_start = cos_val;
        sin_start = sin_val;

        in_out += PF_SHIFT_LIMITED_UNROLL_SIZE;
        N_cplx -= PF_SHIFT_LIMITED_UNROLL_SIZE;
    }
    // "phase_state := starts"
    d->complex_phase.i = cos_start;
    d->complex_phase.q = sin_start;
}


#ifdef HAVE_SSE_INTRINSICS

/*********************************************************************/

/**************/
/*** ALGO F ***/
/**************/

shift_limited_unroll_A_sse_data_t shift_limited_unroll_A_sse_init(float relative_freq, float phase_start_rad)
{
    shift_limited_unroll_A_sse_data_t output;
    float myphase;

    output.phase_increment = 2*relative_freq*PI;

    myphase = 0.0F;
    for (int i = 0; i < PF_SHIFT_LIMITED_UNROLL_SIZE + PF_SHIFT_LIMITED_SIMD_SZ; i += PF_SHIFT_LIMITED_SIMD_SZ)
    {
        for (int k = 0; k < PF_SHIFT_LIMITED_SIMD_SZ; k++)
        {
            myphase += output.phase_increment;
            while(myphase>PI) myphase-=2*PI;
            while(myphase<-PI) myphase+=2*PI;
        }
        output.dcos[i] = cosf(myphase);
        output.dsin[i] = sinf(myphase);
        for (int k = 1; k < PF_SHIFT_LIMITED_SIMD_SZ; k++)
        {
            output.dcos[i+k] = output.dcos[i];
            output.dsin[i+k] = output.dsin[i];
        }
    }

    output.dcos_blk = 0.0F;
    output.dsin_blk = 0.0F;

    myphase = phase_start_rad;
    for (int i = 0; i < PF_SHIFT_LIMITED_SIMD_SZ; i++)
    {
        output.phase_state_i[i] = cosf(myphase);
        output.phase_state_q[i] = sinf(myphase);
        myphase += output.phase_increment;
        while(myphase>PI) myphase-=2*PI;
        while(myphase<-PI) myphase+=2*PI;
    }
    return output;
}


PF_TARGET_CLONES
void shift_limited_unroll_A_sse_inp_c(complexf* in_out, int N_cplx, shift_limited_unroll_A_sse_data_t* d)
{
    // "vals := starts := phase_state"
    __m128 cos_starts = VLOAD( &d->phase_state_i[0] );
    __m128 sin_starts = VLOAD( &d->phase_state_q[0] );
    __m128 cos_vals = cos_starts;
    __m128 sin_vals = sin_starts;
    __m128 inp_re, inp_im;
    __m128 product_re, product_im;
    __m128 interl_prod_a, interl_prod_b;
    __m128 * RESTRICT p_trig_cos_tab;
    __m128 * RESTRICT p_trig_sin_tab;
    __m128 * RESTRICT u = (__m128*)in_out;

    while (N_cplx)
    {
        const int NB = (N_cplx >= PF_SHIFT_LIMITED_UNROLL_SIZE) ? PF_SHIFT_LIMITED_UNROLL_SIZE : N_cplx;
        int B = NB;
        p_trig_cos_tab = (__m128*)( &d->dcos[0] );
        p_trig_sin_tab = (__m128*)( &d->dsin[0] );
        while (B)
        {
            // complex multiplication of 4 complex values from/to in_out[]
            // ==  u[0..3] *= (cos_val[0..3] + i * sin_val[0..3]):
            // "out[] = inp[] * vals"
            UNINTERLEAVE2(VLOAD(u), VLOAD(u+1), inp_re, inp_im);  /* inp_re = all reals; inp_im = all imags */
            product_re = VSUB( VMUL(inp_re, cos_vals), VMUL(inp_im, sin_vals) );
            product_im = VADD( VMUL(inp_im, cos_vals), VMUL(inp_re, sin_vals) );
            INTERLEAVE2( product_re, product_im, interl_prod_a, interl_prod_b);
            VSTORE(u, interl_prod_a);
            VSTORE(u+1, interl_prod_b);
            u += 2;
            // calculate complex phasor for next iteration
            // cos_val = cos_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] - sin_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            // sin_val = sin_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] + cos_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            // cos_val[]/sin_val[] .. can't fade towards 0 inside this while loop :-)
            // "vals :=  d[] * starts"
            inp_re = VLOAD(p_trig_cos_tab);
            inp_im = VLOAD(p_trig_sin_tab);
            cos_vals = VSUB( VMUL(inp_re, cos_starts), VMUL(inp_im, sin_starts) );
            sin_vals = VADD( VMUL(inp_im, cos_starts), VMUL(inp_re, sin_starts) );
            ++p_trig_cos_tab;
            ++p_trig_sin_tab;
            B -= 4;
        }
        N_cplx -= NB;
        /* normalize d->phase_state_i[]/d->phase_state_q[], that magnitude does not fade towards 0 ! */
        /* re-use product_re[]/product_im[] for normalization */
        // "starts := vals := vals / |vals|"
        product_re = VADD( VMUL(cos_vals, cos_vals), VMUL(sin_vals, sin_vals) );
#if 0
        // more spikes in spectrum! at PF_SHIFT_LIMITED_UNROLL_SIZE = 64
        // higher spikes in spectrum at PF_SHIFT_LIMITED_UNROLL_SIZE = 16
        product_im = _mm_rsqrt_ps(product_re);
        cos_starts = cos_vals = VMUL(cos_vals, product_im);
        sin_starts = sin_vals = VMUL(sin_vals, product_im);
#else
        // spectrally comparable to shift_match_cc() with PF_SHIFT_LIMITED_UNROLL_SIZE = 64 - but slower!
        // spectrally comparable to shift_match_cc() with PF_SHIFT_LIMITED_UNROLL_SIZE = 128 - fast again
        product_im = _mm_sqrt_ps(product_re);
        cos_starts = cos_vals = VDIV(cos_vals, product_im);
        sin_starts = sin_vals = VDIV(sin_vals, product_im);
#endif
    }
    // "phase_state := starts"
    VSTORE( &d->phase_state_i[0], cos_starts );
    VSTORE( &d->phase_state_q[0], sin_starts );
}


/*********************************************************************/

/**************/
/*** ALGO G ***/
/**************/

shift_limited_unroll_B_sse_data_t shift_limited_unroll_B_sse_init(float relative_freq, float phase_start_rad)
{
    shift_limited_unroll_B_sse_data_t output;
    float myphase;

    output.phase_increment = 2*relative_freq*PI;

    myphase = 0.0F;
    for (int i = 0; i < PF_SHIFT_LIMITED_UNROLL_SIZE + PF_SHIFT_LIMITED_SIMD_SZ; i += PF_SHIFT_LIMITED_SIMD_SZ)
    {
        for (int k = 0; k < PF_SHIFT_LIMITED_SIMD_SZ; k++)
        {
            myphase += output.phase_increment;
            while(myphase>PI) myphase-=2*PI;
            while(myphase<-PI) myphase+=2*PI;
        }
        output.dtrig[i+0] = cosf(myphase);
        output.dtrig[i+1] = sinf(myphase);
        output.dtrig[i+2] = output.dtrig[i+0];
        output.dtrig[i+3] = output.dtrig[i+1];
    }

    output.dcos_blk = 0.0F;
    output.dsin_blk = 0.0F;

    myphase = phase_start_rad;
    for (int i = 0; i < PF_SHIFT_LIMITED_SIMD_SZ; i++)
    {
        output.phase_state_i[i] = cosf(myphase);
        output.phase_state_q[i] = sinf(myphase);
        myphase += output.phase_increment;
        while(myphase>PI) myphase-=2*PI;
        while(myphase<-PI) myphase+=2*PI;
    }
    return output;
}


PF_TARGET_CLONES
void shift_limited_unroll_B_sse_inp_c(complexf* in_out, int N_cplx, shift_limited_unroll_B_sse_data_t* d)
{
    // "vals := starts := phase_state"
    __m128 cos_starts = VLOAD( &d->phase_state_i[0] );
    __m128 sin_starts = VLOAD( &d->phase_state_q[0] );
    __m128 cos_vals = cos_starts;
    __m128 sin_vals = sin_starts;
    __m128 inp_re, inp_im;
    __m128 product_re, product_im;
    __m128 interl_prod_a, interl_prod_b;
    __m128 * RESTRICT p_trig_tab;
    __m128 * RESTRICT u = (__m128*)in_out;

    while (N_cplx)
    {
        const int NB = (N_cplx >= PF_SHIFT_LIMITED_UNROLL_SIZE) ? PF_SHIFT_LIMITED_UNROLL_SIZE : N_cplx;
        int B = NB;
        p_trig_tab = (__m128*)( &d->dtrig[0] );
        while (B)
        {
            // complex multiplication of 4 complex values from/to in_out[]
            // ==  u[0..3] *= (cos_val[0..3] + i * sin_val[0..3]):
            // "out[] = inp[] * vals"
            UNINTERLEAVE2(VLOAD(u), VLOAD(u+1), inp_re, inp_im);  /* inp_re = all reals; inp_im = all imags */
            product_re = VSUB( VMUL(inp_re, cos_vals), VMUL(inp_im, sin_vals) );
            product_im = VADD( VMUL(inp_im, cos_vals), VMUL(inp_re, sin_vals) );
            INTERLEAVE2( product_re, product_im, interl_prod_a, interl_prod_b);
            VSTORE(u, interl_prod_a);
            VSTORE(u+1, interl_prod_b);
            u += 2;
            // calculate complex phasor for next iteration
            // cos_val = cos_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] - sin_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            // sin_val = sin_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] + cos_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            // cos_val[]/sin_val[] .. can't fade towards 0 inside this while loop :-)
            // "vals :=  d[] * starts"
            product_re = VLOAD(p_trig_tab);
            UNINTERLEAVE2(product_re, product_re, inp_re, inp_im);  /* inp_re = all reals; inp_im = all imags */
            cos_vals = VSUB( VMUL(inp_re, cos_starts), VMUL(inp_im, sin_starts) );
            sin_vals = VADD( VMUL(inp_im, cos_starts), VMUL(inp_re, sin_starts) );
            ++p_trig_tab;
            B -= 4;
        }
        N_cplx -= NB;
        /* normalize d->phase_state_i[]/d->phase_state_q[], that magnitude does not fade towards 0 ! */
        /* re-use product_re[]/product_im[] for normalization */
        // "starts := vals := vals / |vals|"
        product_re = VADD( VMUL(cos_vals, cos_vals), VMUL(sin_vals, sin_vals) );
#if 0
        // more spikes in spectrum! at PF_SHIFT_LIMITED_UNROLL_SIZE = 64
        // higher spikes in spectrum at PF_SHIFT_LIMITED_UNROLL_SIZE = 16
        product_im = _mm_rsqrt_ps(product_re);
        cos_starts = cos_vals = VMUL(cos_vals, product_im);
        sin_starts = sin_vals = VMUL(sin_vals, product_im);
#else
        // spectrally comparable to shift_match_cc() with PF_SHIFT_LIMITED_UNROLL_SIZE = 64 - but slower!
        // spectrally comparable to shift_match_cc() with PF_SHIFT_LIMITED_UNROLL_SIZE = 128 - fast again
        product_im = _mm_sqrt_ps(product_re);
        cos_starts = cos_vals = VDIV(cos_vals, product_im);
        sin_starts = sin_vals = VDIV(sin_vals, product_im);
#endif
    }
    // "phase_state := starts"
    VSTORE( &d->phase_state_i[0], cos_starts );
    VSTORE( &d->phase_state_q[0], sin_starts );
}


/*********************************************************************/


/**************/
/*** ALGO H ***/
/**************/

shift_limited_unroll_C_sse_data_t shift_limited_unroll_C_sse_init(float relative_freq, float phase_start_rad)
{
    shift_limited_unroll_C_sse_data_t output;
    float myphase;

    output.phase_increment = 2*relative_freq*PI;

    myphase = 0.0F;
    for (int i = 0; i < PF_SHIFT_LIMITED_UNROLL_SIZE + PF_SHIFT_LIMITED_SIMD_SZ; i += PF_SHIFT_LIMITED_SIMD_SZ)
    {
        for (int k = 0; k < PF_SHIFT_LIMITED_SIMD_SZ; k++)
        {
            myphase += output.phase_increment;
            while(myphase>PI) myphase-=2*PI;
            while(myphase<-PI) myphase+=2*PI;
        }
        output.dinterl_trig[2*i] = cosf(myphase);
        output.dinterl_trig[2*i+4] = sinf(myphase);
        for (int k = 1; k < PF_SHIFT_LIMITED_SIMD_SZ; k++)
        {
            output.dinterl_trig[2*i+k] = output.dinterl_trig[2*i];
            output.dinterl_trig[2*i+k+4] = output.dinterl_trig[2*i+4];
        }
    }

    output.dcos_blk = 0.0F;
    output.dsin_blk = 0.0F;

    myphase = phase_start_rad;
    for (int i = 0; i < PF_SHIFT_LIMITED_SIMD_SZ; i++)
    {
        output.phase_state_i[i] = cosf(myphase);
        output.phase_state_q[i] = sinf(myphase);
        myphase += output.phase_increment;
        while(myphase>PI) myphase-=2*PI;
        while(myphase<-PI) myphase+=2*PI;
    }
    return output;
}


PF_TARGET_CLONES
void shift_limited_unroll_C_sse_inp_c(complexf* in_out, int N_cplx, shift_limited_unroll_C_sse_data_t* d)
{
    // "vals := starts := phase_state"
    __m128 cos_starts = VLOAD( &d->phase_state_i[0] );
    __m128 sin_starts = VLOAD( &d->phase_state_q[0] );
    __m128 cos_vals = cos_starts;
    __m128 sin_vals = sin_starts;
    __m128 inp_re, inp_im;
    __m128 product_re, product_im;
    __m128 interl_prod_a, interl_prod_b;
    __m128 * RESTRICT p_trig_tab;
    __m128 * RESTRICT u = (__m128*)in_out;

    while (N_cplx)
    {
        const int NB = (N_cplx >= PF_SHIFT_LIMITED_UNROLL_SIZE) ? PF_SHIFT_LIMITED_UNROLL_SIZE : N_cplx;
        int B = NB;
        p_trig_tab = (__m128*)( &d->dinterl_trig[0] );
        while (B)
        {
            // complex multiplication of 4 complex values from/to in_out[]
            // ==  u[0..3] *= (cos_val[0..3] + i * sin_val[0..3]):
            // "out[] = inp[] * vals"
            UNINTERLEAVE2(VLOAD(u), VLOAD(u+1), inp_re, inp_im);  /* inp_re = all reals; inp_im = all imags */
            product_re = VSUB( VMUL(inp_re, cos_vals), VMUL(inp_im, sin_vals) );
            product_im = VADD( VMUL(inp_im, cos_vals), VMUL(inp_re, sin_vals) );
            INTERLEAVE2( product_re, product_im, interl_prod_a, interl_prod_b);
            VSTORE(u, interl_prod_a);
            VSTORE(u+1, interl_prod_b);
            u += 2;
            // calculate complex phasor for next iteration
            // cos_val = cos_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] - sin_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            // sin_val = sin_start * d->dcos[PF_SHIFT_LIMITED_SIMD_SZ*i+j] + cos_start * d->dsin[PF_SHIFT_LIMITED_SIMD_SZ*i+j];
            // cos_val[]/sin_val[] .. can't fade towards 0 inside this while loop :-)
            // "vals :=  d[] * starts"
            inp_re = VLOAD(p_trig_tab);
            inp_im = VLOAD(p_trig_tab+1);
            cos_vals = VSUB( VMUL(inp_re, cos_starts), VMUL(inp_im, sin_starts) );
            sin_vals = VADD( VMUL(inp_im, cos_starts), VMUL(inp_re, sin_starts) );
            p_trig_tab += 2;
            B -= 4;
        }
        N_cplx -= NB;
        /* normalize d->phase_state_i[]/d->phase_state_q[], that magnitude does not fade towards 0 ! */
        /* re-use product_re[]/product_im[] for normalization */
        // "starts := vals := vals / |vals|"
        product_re = VADD( VMUL(cos_vals, cos_vals), VMUL(sin_vals, sin_vals) );
#if 0
        // more spikes in spectrum! at PF_SHIFT_LIMITED_UNROLL_SIZE = 64
        // higher spikes in spectrum at PF_SHIFT_LIMITED_UNROLL_SIZE = 16
        product_im = _mm_rsqrt_ps(product_re);
        cos_starts = cos_vals = VMUL(cos_vals, product_im);
        sin_starts = sin_vals = VMUL(sin_vals, product_im);
#else
        // spectrally comparable to shift_match_cc() with PF_SHIFT_LIMITED_UNROLL_SIZE = 64 - but slower!
        // spectrally comparable to shift_match_cc() with PF_SHIFT_LIMITED_UNROLL_SIZE = 128 - fast again
        product_im = _mm_sqrt_ps(product_re);
        cos_starts = cos_vals = VDIV(cos_vals, product_im);
        sin_starts = sin_vals = VDIV(sin_vals, product_im);
#endif
    }
    // "phase_state := starts"
    VSTORE( &d->phase_state_i[0], cos_starts );
    VSTORE( &d->phase_state_q[0], sin_starts );
}


#else

/*********************************************************************/

shift_limited_unroll_A_sse_data_t shift_limited_unroll_A_sse_init(float relative_freq, float phase_start_rad) {
    assert(0);
    shift_limited_unroll_A_sse_data_t r;
    return r;
}
shift_limited_unroll_B_sse_data_t shift_limited_unroll_B_sse_init(float relative_freq, float phase_start_rad) {
    assert(0);
    shift_limited_unroll_B_sse_data_t r;
    return r;
}
shift_limited_unroll_C_sse_data_t shift_limited_unroll_C_sse_init(float relative_freq, float phase_start_rad) {
    assert(0);
    shift_limited_unroll_C_sse_data_t r;
    return r;
}

void shift_limited_unroll_A_sse_inp_c(complexf* in_out, int N_cplx, shift_limited_unroll_A_sse_data_t* d) {
    assert(0);
}
void shift_limited_unroll_B_sse_inp_c(complexf* in_out, int N_cplx, shift_limited_unroll_B_sse_data_t* d) {
    assert(0);
}
void shift_limited_unroll_C_sse_inp_c(complexf* in_out, int N_cplx, shift_limited_unroll_C_sse_data_t* d) {
    assert(0);
}

#endif


/*********************************************************************/

/**************/
/*** ALGO I ***/
/**************/

void shift_recursive_osc_update_rate(float rate, shift_recursive_osc_conf_t *conf, shift_recursive_osc_t* state)
{
    // constants for single phase step
    float phase_increment_s = rate*PI;
    float k1 = tanf(0.5f*phase_increment_s);
    float k2 = 2*k1 /(1 + k1 * k1);
    for (int j=1; j<PF_SHIFT_RECURSIVE_SIMD_SZ; j++)
    {
        float tmp;
        state->u_cos[j] = state->u_cos[j-1];
        state->v_sin[j] = state->v_sin[j-1];
        // small steps
        tmp = state->u_cos[j] - k1 * state->v_sin[j];
        state->v_sin[j] += k2 * tmp;
        state->u_cos[j] = tmp - k1 * state->v_sin[j];
    }

    // constants for PF_SHIFT_RECURSIVE_SIMD_SZ times phase step
    float phase_increment_b = phase_increment_s * PF_SHIFT_RECURSIVE_SIMD_SZ;
    while(phase_increment_b > PI) phase_increment_b-=2*PI;
    while(phase_increment_b < -PI) phase_increment_b+=2*PI;
    conf->k1 = tanf(0.5f*phase_increment_b);
    conf->k2 = 2*conf->k1 / (1 + conf->k1 * conf->k1);
}

void shift_recursive_osc_init(float rate, float starting_phase, shift_recursive_osc_conf_t *conf, shift_recursive_osc_t *state)
{
    if (starting_phase != 0.0F)
    {
        state->u_cos[0] = cosf(starting_phase);
        state->v_sin[0] = sinf(starting_phase);
    }
    else
    {
        state->u_cos[0] = 1.0F;
        state->v_sin[0] = 0.0F;
    }
    shift_recursive_osc_update_rate(rate, conf, state);
}


PF_TARGET_CLONES
void shift_recursive_osc_cc(const complexf *input, complexf* output,
    int size, const shift_recursive_osc_conf_t *conf, shift_recursive_osc_t* state_ext)
{
    float tmp[PF_SHIFT_RECURSIVE_SIMD_SZ];
    float inp_i[PF_SHIFT_RECURSIVE_SIMD_SZ];
    float inp_q[PF_SHIFT_RECURSIVE_SIMD_SZ];
    shift_recursive_osc_t state = *state_ext;
    const float k1 = conf->k1;
    const float k2 = conf->k2;
    for(int i=0;i<size/PF_SHIFT_RECURSIVE_SIMD_SZ; i++) //@shift_recursive_osc_cc
    {
        //we multiply two complex numbers - similar to shift_math_cc
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
        {
            inp_i[j] = input[PF_SHIFT_RECURSIVE_SIMD_SZ*i+j].i;
            inp_q[j] = input[PF_SHIFT_RECURSIVE_SIMD_SZ*i+j].q;
        }
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
        {
            iof(output,PF_SHIFT_RECURSIVE_SIMD_SZ*i+j) = state.u_cos[j] * inp_i[j] - state.v_sin[j] * inp_q[j];
            qof(output,PF_SHIFT_RECURSIVE_SIMD_SZ*i+j) = state.v_sin[j] * inp_i[j] + state.u_cos[j] * inp_q[j];
        }
        // update complex phasor - like incrementing phase
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            tmp[j] = state.u_cos[j] - k1 * state.v_sin[j];
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            state.v_sin[j] += k2 * tmp[j];
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            state.u_cos[j] = tmp[j] - k1 * state.v_sin[j];
    }
    *state_ext = state;
}

PF_TARGET_CLONES
void shift_recursive_osc_inp_c(complexf* in_out,
    int size, const shift_recursive_osc_conf_t *conf, shift_recursive_osc_t* state_ext)
{
    float tmp[PF_SHIFT_RECURSIVE_SIMD_SZ];
    float inp_i[PF_SHIFT_RECURSIVE_SIMD_SZ];
    float inp_q[PF_SHIFT_RECURSIVE_SIMD_SZ];
    shift_recursive_osc_t state = *state_ext;
    const float k1 = conf->k1;
    const float k2 = conf->k2;
    for(int i=0;i<size/PF_SHIFT_RECURSIVE_SIMD_SZ; i++) //@shift_recursive_osc_inp_c
    {
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
        {
            inp_i[j] = in_out[PF_SHIFT_RECURSIVE_SIMD_SZ*i+j].i;
            inp_q[j] = in_out[PF_SHIFT_RECURSIVE_SIMD_SZ*i+j].q;
        }
        //we multiply two complex numbers - similar to shift_math_cc
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
        {
            iof(in_out,PF_SHIFT_RECURSIVE_SIMD_SZ*i+j) = state.u_cos[j] * inp_i[j] - state.v_sin[j] * inp_q[j];
            qof(in_out,PF_SHIFT_RECURSIVE_SIMD_SZ*i+j) = state.v_sin[j] * inp_i[j] + state.u_cos[j] * inp_q[j];
        }
        // update complex phasor - like incrementing phase
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            tmp[j] = state.u_cos[j] - k1 * state.v_sin[j];
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            state.v_sin[j] += k2 * tmp[j];
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            state.u_cos[j] = tmp[j] - k1 * state.v_sin[j];
    }
    *state_ext = state;
}

PF_TARGET_CLONES
void gen_recursive_osc_c(complexf* output,
    int size, const shift_recursive_osc_conf_t *conf, shift_recursive_osc_t* state_ext)
{
    float tmp[PF_SHIFT_RECURSIVE_SIMD_SZ];
    shift_recursive_osc_t state = *state_ext;
    const float k1 = conf->k1;
    const float k2 = conf->k2;
    for(int i=0;i<size/PF_SHIFT_RECURSIVE_SIMD_SZ; i++) //@gen_recursive_osc_c
    {
        // output complex oscillator value
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
        {
            iof(output,PF_SHIFT_RECURSIVE_SIMD_SZ*i+j) = state.u_cos[j];
            qof(output,PF_SHIFT_RECURSIVE_SIMD_SZ*i+j) = state.v_sin[j];
        }
        // update complex phasor - like incrementing phase
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            tmp[j] = state.u_cos[j] - k1 * state.v_sin[j];
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            state.v_sin[j] += k2 * tmp[j];
        for (int j=0;j<PF_SHIFT_RECURSIVE_SIMD_SZ;j++)
            state.u_cos[j] = tmp[j] - k1 * state.v_sin[j];
    }
    *state_ext = state;
}


#ifdef HAVE_SSE_INTRINSICS

/*********************************************************************/

/**************/
/*** ALGO J ***/
/**************/

void shift_recursive_osc_sse_update_rate(float rate, shift_recursive_osc_sse_conf_t *conf, shift_recursive_osc_sse_t* state)
{
    // constants for single phase step
    float phase_increment_s = rate*PI;
    float k1 = tanf(0.5f*phase_increment_s);
    float k2 = 2*k1 /(1 + k1 * k1);
    for (int j=1; j<PF_SHIFT_RECURSIVE_SIMD_SSE_SZ; j++)
    {
        float tmp;
        state->u_cos[j] = state->u_cos[j-1];
        state->v_sin[j] = state->v_sin[j-1];
        // small steps
        tmp = state->u_cos[j] - k1 * state->v_sin[j];
        state->v_sin[j] += k2 * tmp;
        state->u_cos[j] = tmp - k1 * state->v_sin[j];
    }

    // constants for PF_SHIFT_RECURSIVE_SIMD_SSE_SZ times phase step
    float phase_increment_b = phase_increment_s * PF_SHIFT_RECURSIVE_SIMD_SSE_SZ;
    while(phase_increment_b > PI) phase_increment_b-=2*PI;
    while(phase_increment_b < -PI) phase_increment_b+=2*PI;
    conf->k1 = tanf(0.5f*phase_increment_b);
    conf->k2 = 2*conf->k1 / (1 + conf->k1 * conf->k1);
}


void shift_recursive_osc_sse_init(float rate, float starting_phase, shift_recursive_osc_sse_conf_t *conf, shift_recursive_osc_sse_t *state)
{
    if (starting_phase != 0.0F)
    {
        state->u_cos[0] = cosf(starting_phase);
        state->v_sin[0] = sinf(starting_phase);
    }
    else
    {
        state->u_cos[0] = 1.0F;
        state->v_sin[0] = 0.0F;
    }
    shift_recursive_osc_sse_update_rate(rate, conf, state);
}


PF_TARGET_CLONES
void shift_recursive_osc_sse_inp_c(complexf* in_out,
    int N_cplx, const shift_recursive_osc_sse_conf_t *conf, shift_recursive_osc_sse_t* state_ext)
{
    const __m128 k1 = LD_PS1( conf->k1 );
    const __m128 k2 = LD_PS1( conf->k2 );
    __m128 u_cos = VLOAD( &state_ext->u_cos[0] );
    __m128 v_sin = VLOAD( &state_ext->v_sin[0] );
    __m128 inp_re, inp_im;
    __m128 product_re, product_im;
    __m128 interl_prod_a, interl_prod_b;
    __m128 * RESTRICT u = (__m128*)in_out;

    while (N_cplx)
    {
        //inp_i[j] = in_out[PF_SHIFT_RECURSIVE_SIMD_SSE_SZ*i+j].i;
        //inp_q[j] = in_out[PF_SHIFT_RECURSIVE_SIMD_SSE_SZ*i+j].q;
        UNINTERLEAVE2(VLOAD(u), VLOAD(u+1), inp_re, inp_im);  /* inp_re = all reals; inp_im = all imags */

        //we multiply two complex numbers - similar to shift_math_cc
        //iof(in_out,PF_SHIFT_RECURSIVE_SIMD_SSE_SZ*i+j) = state.u_cos[j] * inp_i[j] - state.v_sin[j] * inp_q[j];
        //qof(in_out,PF_SHIFT_RECURSIVE_SIMD_SSE_SZ*i+j) = state.v_sin[j] * inp_i[j] + state.u_cos[j] * inp_q[j];
        product_re = VSUB( VMUL(inp_re, u_cos), VMUL(inp_im, v_sin) );
        product_im = VADD( VMUL(inp_im, u_cos), VMUL(inp_re, v_sin) );
        INTERLEAVE2( product_re, product_im, interl_prod_a, interl_prod_b);
        VSTORE(u, interl_prod_a);
        VSTORE(u+1, interl_prod_b);
        u += 2;

        // update complex phasor - like incrementing phase
        // tmp[j] = state.u_cos[j] - k1 * state.v_sin[j];
        product_re = VSUB( u_cos, VMUL(k1, v_sin) );
        // state.v_sin[j] += k2 * tmp[j];
        v_sin = VADD( v_sin, VMUL(k2, product_re) );
        // state.u_cos[j] = tmp[j] - k1 * state.v_sin[j];
        u_cos = VSUB( product_re, VMUL(k1, v_sin) );

        N_cplx -= 4;
    }
    VSTORE( &state_ext->u_cos[0], u_cos );
    VSTORE( &state_ext->v_sin[0], v_sin );
}

#else

void shift_recursive_osc_sse_update_rate(float rate, shift_recursive_osc_sse_conf_t *conf, shift_recursive_osc_sse_t* state)
{
    assert(0);
}

void shift_recursive_osc_sse_init(float rate, float starting_phase, shift_recursive_osc_sse_conf_t *conf, shift_recursive_osc_sse_t *state)
{
    assert(0);
}


void shift_recursive_osc_sse_inp_c(complexf* in_out,
    int N_cplx, const shift_recursive_osc_sse_conf_t *conf, shift_recursive_osc_sse_t* state_ext)
{
    assert(0);
}

#endif

