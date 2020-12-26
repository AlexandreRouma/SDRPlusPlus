#include "imgui_custom.h"
#include "imgui.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

namespace ImGui{
     void VolumeBar(float volumeL, float volumeR, float *holdL, float *holdR, const ImVec2& size_arg){
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
        ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0))
            return;

        float fractionU = ImSaturate(1.0f+20*std::log10(volumeL)/60.0f);
        float fractionD = ImSaturate(1.0f+20*std::log10(volumeR)/60.0f);

        *holdL = std::max(*holdL-0.005f,fractionU);
        *holdR = std::max(*holdR-0.005f,fractionD);
        
        //Background
        ImRect upbb(bb.Min, ImVec2(bb.Max.x, ImLerp(bb.Min.y, bb.Max.y, 0.4f)));
        RenderRectFilledRangeH(window->DrawList, upbb, 0xff005500, 0.0f, 0.6f, 0.0f);
        RenderRectFilledRangeH(window->DrawList, upbb, 0xff005555, 0.6f, 0.85f, 0.0f);
        RenderRectFilledRangeH(window->DrawList, upbb, 0xff000055, 0.85f, 1.0f, 0.0f);

        ImRect dwbb(ImVec2(bb.Min.x, ImLerp(bb.Min.y, bb.Max.y, 0.6f)), bb.Max);
        RenderRectFilledRangeH(window->DrawList, dwbb, 0xff005500, 0.00f, 0.6f, 0.0f);
        RenderRectFilledRangeH(window->DrawList, dwbb, 0xff005555, 0.60f, 0.85f, 0.0f);
        RenderRectFilledRangeH(window->DrawList, dwbb, 0xff000055, 0.85f, 1.0f, 0.0f);

        //Bar
        RenderRectFilledRangeH(window->DrawList, upbb, 0xff00ff00, 0.00f, 0.60f*ImSaturate(fractionU/0.6f), 0.0f);
        RenderRectFilledRangeH(window->DrawList, upbb, 0xff00ffff, 0.60f, 0.60f+0.25f*ImSaturate((fractionU-0.6f)/0.25f), 0.0f);
        RenderRectFilledRangeH(window->DrawList, upbb, 0xff0000ff, 0.85f, 0.85f+0.15f*ImSaturate((fractionU-0.85f)/0.15f), 0.0f);

        RenderRectFilledRangeH(window->DrawList, dwbb, 0xff00ff00, 0.00f, 0.60f*ImSaturate(fractionD/0.6f), 0.0f);
        RenderRectFilledRangeH(window->DrawList, dwbb, 0xff00ffff, 0.60f, 0.60f+0.25f*ImSaturate((fractionD-0.6f)/0.25f), 0.0f);
        RenderRectFilledRangeH(window->DrawList, dwbb, 0xff0000ff, 0.85f, 0.85f+0.15f*ImSaturate((fractionD-0.85f)/0.15f), 0.0f);

        //Holders
        RenderRectFilledRangeH(window->DrawList, upbb, GetColorU32(ImGuiCol_SliderGrab), *holdL, *holdL+0.01f, 0.0f);

        RenderRectFilledRangeH(window->DrawList, dwbb, GetColorU32(ImGuiCol_SliderGrab), *holdR, *holdR+0.01f, 0.0f);

     }
}
