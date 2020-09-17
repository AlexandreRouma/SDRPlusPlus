#pragma once

namespace dsp {
    struct complex_t {
        float q;
        float i;

        complex_t operator+(complex_t& c) {
            complex_t res;
            res.i = c.i + i;
            res.q = c.q + q;
            return res;
        }

        complex_t operator-(complex_t& c) {
            complex_t res;
            res.i = i - c.i;
            res.q = q - c.q;
            return res;
        }

        complex_t operator*(float& f) {
            complex_t res;
            res.i = i * f;
            res.q = q * f;
            return res;
        }
    };

    struct StereoFloat_t {
        float l;
        float r;

        StereoFloat_t operator+(StereoFloat_t& s) {
            StereoFloat_t res;
            res.l = s.l + l;
            res.r = s.r + r;
            return res;
        }

        StereoFloat_t operator-(StereoFloat_t& s) {
            StereoFloat_t res;
            res.l = l - s.l;
            res.r = r - s.r;
            return res;
        }

        StereoFloat_t operator*(float& f) {
            StereoFloat_t res;
            res.l = l * f;
            res.r = r * f;
            return res;
        }
    };
};