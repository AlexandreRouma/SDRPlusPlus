#ifdef _MSC_VER
#include <tmmintrin.h>
#else
#include <x86intrin.h>
#endif

#include <cdsp/types.h>


inline void cm_mul(__m128& ab, const __m128& xy)
{
    //const __m128 aa = _mm_shuffle_ps(ab, ab, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128 aa = _mm_moveldup_ps(ab);
    const __m128 bb = _mm_movehdup_ps(ab);
    //const __m128 bb = _mm_shuffle_ps(ab, ab, _MM_SHUFFLE(3, 3, 1, 1));
    const __m128 yx = _mm_shuffle_ps(xy, xy, _MM_SHUFFLE(2, 3, 0, 1));
 
    const __m128 tmp = _mm_addsub_ps(_mm_mul_ps(aa, xy), _mm_mul_ps(bb, yx));
    ab = tmp;
}
 
inline void do_mul(cdsp::complex_t* a, const cdsp::complex_t* b, int n)
{
    const int vector_size = 16;
    int simd_iterations = n - (n % vector_size);
    //assert(simd_iterations % vector_size == 0);
 
    for (int i = 0; i < simd_iterations; i += vector_size)
    {
        //__builtin_prefetch(a + i*4 + 64, 0);
        //__builtin_prefetch(b + i*4 + 64, 0);
 
        __m128 vec_a = _mm_load_ps((float*)&a[i]);
        __m128 vec_b = _mm_load_ps((float*)&b[i]);
 
        __m128 vec_a2 = _mm_load_ps((float*)&a[i+2]);
        __m128 vec_b2 = _mm_load_ps((float*)&b[i+2]);
 
        __m128 vec_a3 = _mm_load_ps((float*)&a[i+4]);
        __m128 vec_b3 = _mm_load_ps((float*)&b[i+4]);
 
        __m128 vec_a4 = _mm_load_ps((float*)&a[i+6]);
        __m128 vec_b4 = _mm_load_ps((float*)&b[i+6]);
 
        __m128 vec_a5 = _mm_load_ps((float*)&a[i+8]);
        __m128 vec_b5 = _mm_load_ps((float*)&b[i+8]);
 
        __m128 vec_a6 = _mm_load_ps((float*)&a[i+10]);
        __m128 vec_b6 = _mm_load_ps((float*)&b[i+10]);
 
        __m128 vec_a7 = _mm_load_ps((float*)&a[i+12]);
        __m128 vec_b7 = _mm_load_ps((float*)&b[i+12]);
 
        __m128 vec_a8 = _mm_load_ps((float*)&a[i+14]);
        __m128 vec_b8 = _mm_load_ps((float*)&b[i+14]);
 
        cm_mul(vec_a, vec_b);
        _mm_store_ps((float*)&a[i], vec_a);
        cm_mul(vec_a2, vec_b2);
        _mm_store_ps((float*)&a[i+2], vec_a2);
        cm_mul(vec_a3, vec_b3);
        _mm_store_ps((float*)&a[i+4], vec_a3);
        cm_mul(vec_a4, vec_b4);
        _mm_store_ps((float*)&a[i+6], vec_a4);
 
        cm_mul(vec_a5, vec_b5);
        _mm_store_ps((float*)&a[i+8], vec_a5);
        cm_mul(vec_a6, vec_b6);
        _mm_store_ps((float*)&a[i+10], vec_a6);
        cm_mul(vec_a7, vec_b7);
        _mm_store_ps((float*)&a[i+12], vec_a7);
        cm_mul(vec_a8, vec_b8);
        _mm_store_ps((float*)&a[i+14], vec_a8);
    }
 
    // finish with scalar
    for (int i = simd_iterations; i < n; i++)
    {
        cdsp::complex_t cm;
        cm.q = a[i].q*b[i].q - a[i].i*b[i].i;
        cm.i = a[i].q*b[i].i + b[i].q*a[i].i;
        a[i] = cm;
    }
}