#pragma once

// Code by: Stellaris

#include <cmath>
#include <complex>
#include <cassert>
#include <cdsp/types.h>

#define M_PI 3.14159265359

#define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )

// Lookup table that store the reverse of each table
uint8_t lut[256] = { R6(0), R6(2), R6(1), R6(3) };

inline uint16_t reverse_16(uint16_t val)
{
    uint8_t lo = lut[val&0xFF];
    uint8_t hi = lut[(val>>8)&0xFF];

    return (lo << 8) | hi;
}

static size_t reverse_bits(size_t x, int n) {
    size_t result = 0;
    for (int i = 0; i < n; i++, x >>= 1)
        result = (result << 1) | (x & 1U);
    return result;
}

// struct complex
// {
//     float re;
//     float im;
// };

inline void bit_reverse_swap_aos(cdsp::complex_t* data, int n)
{
    assert(n < 65536); // only up to 16-bit size
    int power2 = 0;
    for (size_t temp = n; temp > 1U; temp >>= 1)
        power2++;

    power2 = 16 - power2;

    for (int i = 0; i < n; i++) {
        int j = reverse_16(i << power2);
        if (j > i) {
            std::swap(data[i], data[j]);
        }
    }
}

struct trig_table
{
    float* cos_table;
    float* sin_table;
};
trig_table tables[14];

trig_table get_trig_table(int power2)
{
    assert(power2 < 14);
    trig_table& table = tables[power2];
    if (table.cos_table == 0)
    {
        int n = 1 << (power2);

        table.cos_table = (float*)malloc((n/2) * sizeof(float));
        table.sin_table = (float*)malloc((n/2) * sizeof(float));

        for (size_t i = 0; i < n / 2; i++) {
            table.cos_table[i] = cos(2 * M_PI * i / n);
            table.sin_table[i] = sin(2 * M_PI * i / n);
        }
    }

    return table;
}

void fft_aos(cdsp::complex_t* data,  int n) {

    int power2 = 0;
    for (size_t temp = n; temp > 1U; temp >>= 1)
        power2++;

    float* cos_table; float* sin_table;
    trig_table table = get_trig_table(power2);
    cos_table = table.cos_table; sin_table = table.sin_table;

    size_t size = (n / 2) * sizeof(float);

    // Bit-reversed addressing permutation
    bit_reverse_swap_aos(data, n);

    // Cooley-Tukey decimation-in-time radix-2 FFT
    for (size_t size = 2; size <= n; size *= 2) {
        size_t halfsize = size / 2;
        size_t tablestep = n / size;
        for (size_t i = 0; i < n; i += size) {
            for (size_t j = i, k = 0; j < i + halfsize; j++, k += tablestep) {
                size_t l = j + halfsize;
                float tpre =  data[l].q * cos_table[k] + data[l].i * sin_table[k];
                float tpim =  data[l].i * cos_table[k] - data[l].q * sin_table[k];
                data[l].q = data[j].q - tpre;
                data[l].i = data[j].i - tpim;
                data[j].q += tpre;
                data[j].i += tpim;
            }
        }
    }
}

// for (int i = 0; i < 327680; i++) {
//     complex cm;
//     cm.q = complexes[i].q*sineGen[i].q - complexes[i].i*sineGen[i].i;
//     cm.i = complexes[i].q*sineGen[i].i + sineGen[i].q*complexes[i].i;
//     complexes[i] = cm;
// }

// ImGui::Begin("FFT");
// ImGui::PlotLines("I", [](void*data, int idx) { return ((float*)data)[idx]; }, endData, 1024, 0, 0, -1, 12, ImVec2(1024, 200));
// ImGui::InputFloat("Freq", &frequency, 100000.0f, 100000.0f);
// ImGui::End();