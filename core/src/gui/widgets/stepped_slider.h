#pragma once

namespace ImGui {
    bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format = "%.3f");
}
