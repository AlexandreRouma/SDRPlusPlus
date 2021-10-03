#include "fir.h"
#include <math.h>

#define K_PI 3.141592653f
#define K_2PI (2*K_PI)

static float Izero(float x)
{
    float x2 = x / 2.0f;
    float sum = 1.0f;
    float ds = 1.0f;
    float di = 1.0f;
    float errorlimit = 1e-9f;
    float tmp;
    do
    {
        tmp = x2 / di;
        tmp *= tmp;
        ds *= tmp;
        sum += ds;
        di += 1.0;
    } while (ds >= errorlimit * sum);
    //qDebug()<<"x="<<x<<" I0="<<sum;
    return(sum);
}


////////////////////////////////////////////////////////////////////
// Create a FIR Low Pass filter
// num_taps if > 0, forces filter design to be this number of taps
//          if < 0, limits filter design to be max negative number of taps
// Astop = Stopband Atenuation in dB (ie 40dB is 40dB stopband attenuation)
// normFpass = Lowpass passband frequency - relative to samplerate
// normFstop = Lowpass stopband frequency - relative to samplerate
// Coef = pointer to array, where to put the resulting (real) coefficients
//        might be nullptr, to estimate the number of coefficients
// return the used/estimated number of coefficients
//
//           -------------
//                        |
//                         |
//                          |
//                           |
//    Astop                   ---------------
//                    Fpass   Fstop
//
////////////////////////////////////////////////////////////////////
int KaiserWindow(int num_taps, float Astop, float normFpass, float normFstop, float *Coef)
{
    int n;
    float Beta;

    float Scale = 1.0f; //+106dB over 1.0 to have high level in wavosaur spectrum analisys out. otherwise set to 1.0
//    float Astop = 100.0f; // we want high attenuation 100 dB

    //create normalized frequency parameters
    float normFcut = (normFstop + normFpass) / 2.0f;   //low pass filter 6dB cutoff

    //calculate Kaiser-Bessel window shape factor, Beta, from stopband attenuation
    if (Astop < 20.96f)
        Beta = 0.0f;
    else if (Astop >= 50.0f)
        Beta = .1102f * (Astop - 8.71f);
    else
        Beta = .5842f * powf((Astop - 20.96f), 0.4f) + .07886f * (Astop - 20.96f);

    /* I used this way but beta is Beta way is better
    float Alpha = 3.5;
    Beta = K_PI * Alpha;
    */

    // now estimate number of filter taps required based on filter specs
    int m_NumTaps = (Astop - 8.0) / (2.285*K_2PI*(normFstop - normFpass) ) + 1;

    // clamp range of filter taps
    if (num_taps < 0 && m_NumTaps > -num_taps)
        m_NumTaps = -num_taps;
    if (m_NumTaps < 3)
        m_NumTaps = 3;

    // early exit, if the user only wanted to estimate the number of taps
    if (num_taps <= 0 && !Coef)
        return m_NumTaps;

    if (num_taps > 0)
        m_NumTaps = num_taps;

    float fCenter = .5f * (float)(m_NumTaps - 1);
    float izb = Izero(Beta);        //precalculate denominator since is same for all points
    for (n = 0; n < m_NumTaps; n++)
    {
        float x = (float)n - fCenter;
        float c;
        // create ideal Sinc() LP filter with normFcut
        if ((float)n == fCenter)   //deal with odd size filter singularity where sin(0)/0==1
            c = 2.0f * normFcut;
        else
            c = (float)sinf(K_2PI * x * normFcut) / (K_PI * x);
        //calculate Kaiser window and multiply to get coefficient
        x = ((float)n - ((float)m_NumTaps - 1.0f) / 2.0f) / (((float)m_NumTaps - 1.0f) / 2.0f);
        Coef[n] = Scale * c * Izero(Beta * sqrtf(1 - (x * x))) / izb;
    }

    return m_NumTaps;
}
