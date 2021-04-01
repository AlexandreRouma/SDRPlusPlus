#pragma once
#include <gui/widgets/constellation_diagram.h>

namespace ImGui {
    ConstellationDiagram::ConstellationDiagram() {
        memset(buffer, 0, 1024 * sizeof(dsp::complex_t));
    }

    void ConstellationDiagram::draw(const ImVec2& size_arg) {
        std::lock_guard<std::mutex> lck(bufferMtx);
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiStyle& style = GetStyle();
        float pad = style.FramePadding.y;
        ImVec2 min = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), CalcItemWidth());
        ImRect bb(min, ImVec2(min.x+size.x, min.y+size.y));
        float lineHeight = size.y;

        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0)) {
            return;
        }

        window->DrawList->AddRectFilled(min, ImVec2(min.x+size.x, min.y+size.y), IM_COL32(0,0,0,255));
        ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark, 0.7f);
        float increment = size.x / 1024.0f;
        for (int i = 0; i < 1024; i++) {
            if (buffer[i].re > 1.5f || buffer[i].re < -1.5f) { continue; }
            if (buffer[i].im > 1.5f || buffer[i].im < -1.5f) { continue; }
            window->DrawList->AddCircleFilled(ImVec2((((buffer[i].re / 1.5f) + 1) * (size.x*0.5f)) + min.x, (((buffer[i].im / 1.5f) + 1) * (size.y*0.5f)) + min.y), 2, col);
        }
    }

    dsp::complex_t* ConstellationDiagram::aquireBuffer() {
        bufferMtx.lock();
        return buffer;
    }

    void ConstellationDiagram::releaseBuffer() {
        bufferMtx.unlock();
    }

}