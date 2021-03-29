#pragma once

namespace dsp {
    struct complex_t {
        complex_t& operator*(const float& b) {
            return complex_t{re*b, im*b};
        }

        complex_t& operator*(const complex_t& b) {
            return complex_t{(re*b.re) - (im*b.im), (im*b.re) + (re*b.im)};
        }

        complex_t& operator+(const complex_t& b) {
            return complex_t{re+b.re, im+b.im};
        }

        complex_t& operator-(const complex_t& b) {
            return complex_t{re-b.re, im-b.im};
        }

        inline complex_t conj() {
            return complex_t{re, -im};
        }

        float re;
        float im;
    };

    struct stereo_t {
        stereo_t& operator*(const float& b) {
            return stereo_t{l*b, r*b};
        }

        stereo_t& operator+(const stereo_t& b) {
            return stereo_t{l+b.l, r+b.r};
        }

        stereo_t& operator-(const stereo_t& b) {
            return stereo_t{l-b.l, r-b.r};
        }

        float l;
        float r;
    };
}