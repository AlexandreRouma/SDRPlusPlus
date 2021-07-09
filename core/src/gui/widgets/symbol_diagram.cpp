#include <gui/widgets/symbol_diagram.h>

namespace ImGui {
    SymbolDiagram::SymbolDiagram(float scale) {
        _scale = scale;
        memset(buffer, 0, 1024 * sizeof(float));
    }

    void SymbolDiagram::draw(const ImVec2& size_arg) {
        std::lock_guard<std::mutex> lck(bufferMtx);
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiStyle& style = GetStyle();
        float pad = style.FramePadding.y;
        ImVec2 min = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), 100);
        ImRect bb(min, ImVec2(min.x+size.x, min.y+size.y));
        float lineHeight = size.y;

        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0)) {
            return;
        }

        window->DrawList->AddRectFilled(min, ImVec2(min.x+size.x, min.y+size.y), IM_COL32(0,0,0,255));
        ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark, 0.7f);
        float increment = size.x / 1024.0f;
        float val;
        for (int i = 0; i < 1024; i++) {
            val = buffer[i] * _scale;
            if (val > 1.0f || val < -1.0f) { continue; }
            window->DrawList->AddCircleFilled(ImVec2(((float)i * increment) + min.x, ((val + 1) * (size.y*0.5f)) + min.y), 2, col);
        }
    }

    float* SymbolDiagram::acquireBuffer() {
        bufferMtx.lock();
        return buffer;
    }

    void SymbolDiagram::releaseBuffer() {
        bufferMtx.unlock();
    }

}