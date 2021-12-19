#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <dsp/stream.h>
#include <mutex>
#include <dsp/types.h>

namespace ImGui {
    class ConstellationDiagram {
    public:
        ConstellationDiagram();

        void draw(const ImVec2& size_arg = ImVec2(0, 0));

        dsp::complex_t* acquireBuffer();

        void releaseBuffer();

    private:
        std::mutex bufferMtx;
        dsp::complex_t buffer[1024];
    };
}