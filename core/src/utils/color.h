#pragma once
#include <algorithm>
#include <math.h>

namespace color {
    inline void RGBtoHSL(float r, float g, float b, float& h, float& s, float& l) {
        // Calculate minimum, maximum and delta of components
        float cmax = std::max<float>(std::max<float>(r, g), b);
        float cmin = std::min<float>(std::min<float>(r, g), b);
        float delta = cmax - cmin;

        // Calculate the hue
        if (delta == 0) { h = 0; }
        else if (r > g && r > b) { h = 60.0f * fmodf((g - b) / delta, 6.0f); }
        else if (g > r && g > b) { h = 60.0f * (((b - r) / delta) + 2.0f); }
        else { h = 60.0f * (((r - g) / delta) + 4.0f); }

        // Calculate lightness
        l = (cmin + cmax) / 2.0f;

        // Calculate saturation
        s = (delta == 0) ? 0 : (delta / (1.0f - fabsf((2.0f * l) - 1.0f)));
    }

    inline void HSLtoRGB(float h, float s, float l, float& r, float& g, float& b) {
        // Calculate coefficients
        float c = s * (1.0f - fabsf((2.0f * l) - 1.0f));
        float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
        float m = l - (c / 2.0f);

        // Affect coefficients to R, G or B depending on hue
        if (h < 60) { r = c; g = x; b = 0; }
        else if (h < 120) { r = x; g = c; b = 0; }
        else if (h < 180) { r = 0; g = c; b = x; }
        else if (h < 240) { r = 0; g = x; b = c; }
        else if (h < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }

        // Add m
        r += m;
        g += m;
        b += m;
    }

    inline void interpRGB(float ar, float ag, float ab, float br, float bg, float bb, float& or, float& og, float& ob, float ratio) {
        or = ar + (br - ar) * ratio;
        og = ag + (bg - ag) * ratio;
        ob = ab + (bb - ab) * ratio;
    }
}