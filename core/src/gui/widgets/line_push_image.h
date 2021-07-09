#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <dsp/stream.h>
#include <mutex>
#include <GL/glew.h>

namespace ImGui {
    class LinePushImage {
    public:
        LinePushImage(int frameWidth, int reservedIncrement);

        void draw(const ImVec2& size_arg = ImVec2(0, 0));

        uint8_t* acquireNextLine(int count = 1);

        void releaseNextLine();

        void clear();

        void save(std::string path);

        int getLineCount();

    private:
        void updateTexture();

        std::mutex bufferMtx;
        uint8_t* frameBuffer;

        int _frameWidth;
        int _reservedIncrement;
        int _lineCount = 0;
        int reservedCount = 0;

        GLuint textureId;

        bool newData = false;
        

    };
}