#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <dsp/stream.h>
#include <mutex>

namespace ImGui {
    class SymbolDiagram {
    public:
        SymbolDiagram();

        void draw(const ImVec2& size_arg = ImVec2(0, 0));

        float* aquireBuffer();

        void releaseBuffer();

    private:
        std::mutex bufferMtx;
        float buffer[1024];

    };
}