#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <dsp/stream.h>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

namespace ImGui {
    class ImageDisplay {
    public:
        ImageDisplay(int width, int height);
        ~ImageDisplay();
        void draw(const ImVec2& size_arg = ImVec2(0, 0));
        void swap();

        void* buffer;

    private:
        void updateTexture();

        std::mutex bufferMtx;
        void* activeBuffer;

        int _width;
        int _height;

        GLuint textureId;

        bool newData = false;
    };
}