#include <gui/widgets/volume_meter.h>
#include <algorithm>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui/imgui_internal.h>

namespace ImGui {
    void VolumeMeter(float avg, float peak, float val_min, float val_max, const ImVec2& size_arg) {
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiStyle& style = GImGui->Style;

        avg = std::clamp<float>(avg, val_min, val_max);
        peak = std::clamp<float>(peak, val_min, val_max);

        ImVec2 min = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), (GImGui->FontSize / 2) + style.FramePadding.y);
        ImRect bb(min, min + size);

        float lineHeight = size.y;

        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0)) {
            return;
        }

        float zeroDb = roundf(((-val_min) / (val_max - val_min)) * size.x);

        window->DrawList->AddRectFilled(min, min + ImVec2(zeroDb, lineHeight), IM_COL32(9, 136, 9, 255));
        window->DrawList->AddRectFilled(min + ImVec2(zeroDb, 0), min + ImVec2(size.x, lineHeight), IM_COL32(136, 9, 9, 255));

        float end = roundf(((avg - val_min) / (val_max - val_min)) * size.x);
        float endP = roundf(((peak - val_min) / (val_max - val_min)) * size.x);

        if (avg <= 0) {
            window->DrawList->AddRectFilled(min, min + ImVec2(end, lineHeight), IM_COL32(0, 255, 0, 255));
        }
        else {
            window->DrawList->AddRectFilled(min, min + ImVec2(zeroDb, lineHeight), IM_COL32(0, 255, 0, 255));
            window->DrawList->AddRectFilled(min + ImVec2(zeroDb, 0), min + ImVec2(end, lineHeight), IM_COL32(255, 0, 0, 255));
        }

        if (peak <= 0) {
            window->DrawList->AddLine(min + ImVec2(endP, -1), min + ImVec2(endP, lineHeight - 1), IM_COL32(127, 255, 127, 255));
        }
        else {
            window->DrawList->AddLine(min + ImVec2(endP, -1), min + ImVec2(endP, lineHeight - 1), IM_COL32(255, 127, 127, 255));
        }
    }
}