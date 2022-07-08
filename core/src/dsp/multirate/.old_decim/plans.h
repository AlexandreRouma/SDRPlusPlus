#pragma once
#include "taps/fir_2_2.h"
#include "taps/fir_4_4.h"
#include "taps/fir_8_4.h"
#include "taps/fir_16_8.h"
#include "taps/fir_32_16.h"
#include "taps/fir_64_32.h"
#include "taps/fir_128_32.h"
#include "taps/fir_256_64.h"
#include "taps/fir_512_128.h"
#include "taps/fir_1024_128.h"
#include "taps/fir_2048_128.h"

namespace dsp::multirate::decim {
    struct stage {
        unsigned int decimation;
        unsigned int tapcount;
        const float* taps;
    };

    const unsigned int plan_2_len = 1;
    const stage plan_2[] = {
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_4_len = 1;
    const stage plan_4[] = {
        { 4, fir_4_4_len, fir_4_4_taps }
    };

    const unsigned int plan_8_len = 2;
    const stage plan_8[] = {
        { 4, fir_8_4_len, fir_8_4_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_16_len = 2;
    const stage plan_16[] = {
        { 8, fir_16_8_len, fir_16_8_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_32_len = 2;
    const stage plan_32[] = {
        { 16, fir_32_16_len, fir_32_16_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_64_len = 2;
    const stage plan_64[] = {
        { 32, fir_64_32_len, fir_64_32_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_128_len = 2;
    const stage plan_128[] = {
        { 32, fir_128_32_len, fir_128_32_taps },
        { 4, fir_4_4_len, fir_4_4_taps }
    };

    const unsigned int plan_256_len = 2;
    const stage plan_256[] = {
        { 64, fir_256_64_len, fir_256_64_taps },
        { 4, fir_4_4_len, fir_4_4_taps }
    };

    const unsigned int plan_512_len = 2;
    const stage plan_512[] = {
        { 128, fir_512_128_len, fir_512_128_taps },
        { 4, fir_4_4_len, fir_4_4_taps }
    };

    const unsigned int plan_1024_len = 3;
    const stage plan_1024[] = {
        { 128, fir_1024_128_len, fir_1024_128_taps },
        { 4, fir_8_4_len, fir_8_4_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_2048_len = 3;
    const stage plan_2048[] = {
        { 128, fir_2048_128_len, fir_2048_128_taps },
        { 8, fir_16_8_len, fir_16_8_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_4096_len = 3;
    const stage plan_4096[] = {
        { 128, fir_2048_128_len, fir_2048_128_taps },
        { 16, fir_32_16_len, fir_32_16_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    const unsigned int plan_8192_len = 3;
    const stage plan_8192[] = {
        { 128, fir_2048_128_len, fir_2048_128_taps },
        { 32, fir_64_32_len, fir_64_32_taps },
        { 2, fir_2_2_len, fir_2_2_taps }
    };

    struct plan {
        unsigned int stageCount;
        const stage* stages;
    };

    const unsigned int plans_len = 13;
    const plan plans[] {
        { plan_2_len, plan_2 },
        { plan_4_len, plan_4 },
        { plan_8_len, plan_8 },
        { plan_16_len, plan_16 },
        { plan_32_len, plan_32 },
        { plan_64_len, plan_64 },
        { plan_128_len, plan_128 },
        { plan_256_len, plan_256 },
        { plan_512_len, plan_512 },
        { plan_1024_len, plan_1024 },
        { plan_2048_len, plan_2048 },
        { plan_4096_len, plan_4096 },
        { plan_8192_len, plan_8192 },
    };
}

/*

Desired ratio:  2
<====== BEST ======>
Stage  0 :  2 : 2  ( 69  taps)
<==================>  4.5464  

Desired ratio:  4
<====== BEST ======>
Stage  0 :  4 : 4  ( 139  taps)
<==================>  4.0912  

Desired ratio:  8
<====== BEST ======>
Stage  0 :  8 : 4  ( 32  taps)
Stage  1 :  2 : 2  ( 69  taps)
<==================>  2.5073

Desired ratio:  16
<====== BEST ======>
Stage  0 :  16 : 8  ( 64  taps)
Stage  1 :  2 : 2  ( 69  taps)
<==================>  1.417775

Desired ratio:  32
<====== BEST ======>
Stage  0 :  32 : 16  ( 128  taps)
Stage  1 :  2 : 2  ( 69  taps)
<==================>  0.897

Desired ratio:  64
<====== BEST ======>
Stage  0 :  64 : 32  ( 254  taps)
Stage  1 :  2 : 2  ( 69  taps)
<==================>  0.6991562499999999

Desired ratio:  128
<====== BEST ======>
Stage  0 :  128 : 32  ( 180  taps)
Stage  1 :  4 : 4  ( 139  taps)
<==================>  0.61851875

Desired ratio:  256
<====== BEST ======>
Stage  0 :  256 : 64  ( 356  taps)
Stage  1 :  4 : 4  ( 139  taps)
<==================>  0.4696125

Desired ratio:  512
<====== BEST ======>
Stage  0 :  512 : 128  ( 711  taps)
Stage  1 :  4 : 4  ( 139  taps)
<==================>  0.38787734375

Desired ratio:  1024
<====== BEST ======>
Stage  0 :  1024 : 128  ( 565  taps)
Stage  1 :  8 : 4  ( 32  taps)
Stage  2 :  2 : 2  ( 69  taps)
<==================>  0.30618515625

Desired ratio:  2048
<====== BEST ======>
Stage  0 :  2048 : 128  ( 514  taps)
Stage  1 :  16 : 8  ( 64  taps)
Stage  2 :  2 : 2  ( 69  taps)
<==================>  0.2665748046875

Desired ratio:  4096
<====== BEST ======>
Stage  0 :  2048 : 128  ( 514  taps)
Stage  1 :  32 : 16  ( 128  taps)
Stage  2 :  2 : 2  ( 69  taps)
<==================>  0.26250625

Desired ratio:  8192
<====== BEST ======>
Stage  0 :  2048 : 128  ( 514  taps)
Stage  1 :  64 : 32  ( 254  taps)
Stage  2 :  2 : 2  ( 69  taps)
<==================>  0.260960595703125

*/