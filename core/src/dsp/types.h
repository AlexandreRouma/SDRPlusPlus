#pragma once
#include <math.h>
#include "math/constants.h"

namespace dsp {
    struct complex_t {
        complex_t operator*(const float b) {
            return complex_t{ re * b, im * b };
        }

        complex_t operator*(const double b) {
            return complex_t{ re * (float)b, im * (float)b };
        }

        complex_t operator/(const float b) {
            return complex_t{ re / b, im / b };
        }

        complex_t operator/(const double b) {
            return complex_t{ re / (float)b, im / (float)b };
        }

        complex_t operator*(const complex_t& b) {
            return complex_t{ (re * b.re) - (im * b.im), (im * b.re) + (re * b.im) };
        }

        complex_t operator+(const complex_t& b) {
            return complex_t{ re + b.re, im + b.im };
        }

        complex_t operator-(const complex_t& b) {
            return complex_t{ re - b.re, im - b.im };
        }

        complex_t& operator+=(const complex_t& b) {
            re += b.re;
            im += b.im;
            return *this;
        }

        complex_t& operator-=(const complex_t& b) {
            re -= b.re;
            im -= b.im;
            return *this;
        }

        complex_t& operator*=(const float& b) {
            re *= b;
            im *= b;
            return *this;
        }

        inline complex_t conj() {
            return complex_t{ re, -im };
        }

        inline float phase() {
            return atan2f(im, re);
        }

        inline float fastPhase() {
            float abs_im = fabsf(im);
            float r, angle;
            if (re == 0.0f && im == 0.0f) { return 0.0f; }
            if (re >= 0.0f) {
                r = (re - abs_im) / (re + abs_im);
                angle = (FL_M_PI / 4.0f) - (FL_M_PI / 4.0f) * r;
            }
            else {
                r = (re + abs_im) / (abs_im - re);
                angle = (3.0f * (FL_M_PI / 4.0f)) - (FL_M_PI / 4.0f) * r;
            }
            if (im < 0.0f) {
                return -angle;
            }
            return angle;
        }

        inline float amplitude() {
            return sqrt((re * re) + (im * im));
        }

        inline float fastAmplitude() {
            float re_abs = fabsf(re);
            float im_abs = fabsf(re);
            if (re_abs > im_abs) { return re_abs + 0.4f * im_abs; }
            return im_abs + 0.4f * re_abs;
        }

        float re;
        float im;
    };

    struct stereo_t {
        stereo_t operator*(const float b) {
            return stereo_t{ l * b, r * b };
        }

        stereo_t operator+(const stereo_t& b) {
            return stereo_t{ l + b.l, r + b.r };
        }

        stereo_t operator-(const stereo_t& b) {
            return stereo_t{ l - b.l, r - b.r };
        }

        stereo_t& operator+=(const stereo_t& b) {
            l += b.l;
            r += b.r;
            return *this;
        }

        stereo_t& operator-=(const stereo_t& b) {
            l -= b.l;
            r -= b.r;
            return *this;
        }

        stereo_t& operator*=(const float& b) {
            l *= b;
            r *= b;
            return *this;
        }

        float l;
        float r;
    };
}