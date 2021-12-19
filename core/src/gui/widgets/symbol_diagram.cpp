#include <gui/widgets/symbol_diagram.h>

namespace ImGui {
    SymbolDiagram::SymbolDiagram(float scale, int count) {
        _scale = scale;
        sampleCount = count;

        buffer = new float[count];

        memset(buffer, 0, sampleCount * sizeof(float));
    }

    SymbolDiagram::~SymbolDiagram() {
        delete[] buffer;
    }

    void SymbolDiagram::draw(const ImVec2& size_arg) {
        std::lock_guard<std::mutex> lck(bufferMtx);
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiStyle& style = GetStyle();
        float pad = style.FramePadding.y;
        ImVec2 min = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), 100);
        ImRect bb(min, ImVec2(min.x + size.x, min.y + size.y));
        float lineHeight = size.y;

        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0)) {
            return;
        }

        window->DrawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), IM_COL32(0, 0, 0, 255));
        ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark, 0.7f);
        ImU32 col2 = ImGui::GetColorU32(ImGuiCol_CheckMark, 0.7f);
        float increment = size.x / (float)sampleCount;
        float val;

        for (auto l : lines) {
            window->DrawList->AddLine(ImVec2(min.x, (((l * _scale) + 1) * (size.y * 0.5f)) + min.y), ImVec2(min.x + size.x, (((l * _scale) + 1) * (size.y * 0.5f)) + min.y), IM_COL32(80, 80, 80, 255));
        }

        for (int i = 0; i < sampleCount; i++) {
            val = buffer[i] * _scale;
            if (val > 1.0f || val < -1.0f) { continue; }
            window->DrawList->AddCircleFilled(ImVec2(((float)i * increment) + min.x, ((val + 1) * (size.y * 0.5f)) + min.y), 2, col);
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