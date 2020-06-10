#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <mutex>
#include <GL/glew.h>

namespace ImGui {
    class WaterFall {
    public:
        WaterFall();

        void draw();
        void pushFFT(std::vector<float> data, int n);

    private:
        void drawWaterfall(ImGuiWindow* window, int width, int height, ImVec2 pos);

        std::vector<std::vector<float>> fftBuffer;
        bool newSamples;
        std::mutex buf_mtx;
        GLuint textureId;
        uint8_t* pixelBuffer;
    };
};