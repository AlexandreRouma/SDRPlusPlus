#pragma once
#include <algorithm>

namespace ImGui {
    template<class T>
    inline bool AllowScrollwheel(T& value, int stepCount, T min, T max) {
        if(!ImGui::IsItemHovered()) { return false; }
        T lastVal = value;
        T step =  (max - min) / (T)stepCount;
        value = std::clamp<T>(value + ((T)ImGui::GetIO().MouseWheel * step), (min < max) ? min : max, (max > min) ? max : min);
        return (value != lastVal);
    }

    template<class T>
    inline bool AllowScrollwheelStSz(T& value, T step, T min, T max) {
        if(!ImGui::IsItemHovered()) { return false; }
        T lastVal = value;
        value = std::clamp<T>(value + ((T)ImGui::GetIO().MouseWheel * step), (min < max) ? min : max, (max > min) ? max : min);
        return (value != lastVal);
    }
}
